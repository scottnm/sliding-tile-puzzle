//
//
//
// WARNING!!!!
// This is prototype code that is not intended to be safe, robust, or rigorous
// It's just a testing ground for me to write out a duct-tape implementation in C.
//
// Given that this is just a toy project to keep me occupied as a hobby, I hope any and all future readers can
// empathize with my desire to not spin endlessly on any good engineering practices here.
//
// ABANDON ALL HOPE YE WHO VENTURE FURTHER
//
//
//

// TODO (scottnm): undo/redo stack?

#include "pch.h"

#define SLEEP_PERIOD_MS 33 // 30 FPS

// 3X3 slide puzzle where each slide piece is 5x5 with a 1 cell buffer around each cell
// width == (bufwidth + cell_width) * num_cells + bufwidth == (1+5)*3 + 1 == 19
// height == width (it's square)
#define FRAME_WIDTH 19
#define FRAME_HEIGHT 19
//   width # of clear cells + a newline for each row
// + 1 for the null terminator
#define FRAME_BUFFER_SIZE (((FRAME_WIDTH + 1) * FRAME_HEIGHT) + 1)
#define GRID_DIMENSION 3 // 3x3 grid of tiles
#define TILE_DIMENSION 5 // each tile is 5x5 set of cells
#define MAX_NEIGHBORS 4 // only cardinal directions are considered

#define CURSORUP_SEQ "\033[A"
#define CURSORUP_SEQ_SIZE (sizeof(CURSORUP_SEQ)-1)

static
void
set_frame_buffer(
    char (*frameBuffer)[FRAME_BUFFER_SIZE],
    size_t row,
    size_t col,
    char value
    )
{
    (*frameBuffer)[row * (FRAME_WIDTH + 1) + col] = value;
}

// FIXME: my style is all kinds of confused. eh do I care? it's a prototype

typedef struct game_state_id_t
{
    uint8_t value;
} game_state_id_t;

typedef struct puzzle_segment_t
{
    bool isSet;
    char imgData[TILE_DIMENSION][TILE_DIMENSION];
} puzzle_segment_t;

static
void
SwapPuzzleSegments(
    _Inout_ puzzle_segment_t* a,
    _Inout_ puzzle_segment_t* b
    )
{
    // a and b cannot overlap
    assert((a <= (b - 1)) ||
           (a >= (b + 1)) );

    puzzle_segment_t tmpTile;
    memcpy(&tmpTile, a, sizeof(tmpTile));
    memcpy(a, b, sizeof(*a));
    memcpy(b, &tmpTile, sizeof(*b));

}

typedef struct puzzle_t
{
    puzzle_segment_t puzzleSegments[GRID_DIMENSION][GRID_DIMENSION];
} puzzle_t;

typedef enum GAME_OVER_STATE
{
    GAME_OVER_STATE_NONE = 0, // game still running
    GAME_OVER_STATE_QUIT = 1,
    GAME_OVER_STATE_WON  = 2,
} GAME_OVER_STATE;

typedef struct game_state_t
{
    game_state_id_t stateId;
    GAME_OVER_STATE gameOverState;
    uint8_t selectedCell[2];
    puzzle_t puzzle;
    puzzle_t puzzleSolution;
} game_state_t;

// FIXME: I would love to get rid of these debug shenanigans
#define DEBUG_LINE_WIDTH 128
// width of the debug line + newline + null-terminator
#define DEBUG_LINE_SIZE (DEBUG_LINE_WIDTH + 1 + 1)
typedef struct debug_render_state_t
{
    bool dirty;
    char clearLine[DEBUG_LINE_SIZE + 1]; // +1 for the backup character
    char backupLine[ARRAYSIZE(CURSORUP_SEQ) + 1 + 1]; // +1 for backup character, +1 for null-terminator
    char debugLine[DEBUG_LINE_SIZE];
} debug_render_state_t;

// FIXME: yikes global shared state. bugs waiting to happen.
static debug_render_state_t g_debugRenderState = {0};

#define WRITE_DEBUG_LINE(FMT, ...) \
    do { \
        debug_render_state_t* rs = &g_debugRenderState; \
        rs->dirty = true; \
        (void)_snprintf_s( \
            rs->debugLine, sizeof(rs->debugLine), sizeof(rs->debugLine), FMT, __VA_ARGS__); \
    } while (0) \

typedef struct render_state_t
{
    game_state_id_t lastRenderedState;

    char clearFrame[FRAME_BUFFER_SIZE];

    //   1 cursor-up character for each line +
    // + 1 backup character once we've reached the top of the line
    // + 1 for the null terminator
    char backupFrame[(FRAME_HEIGHT * CURSORUP_SEQ_SIZE) + 1 + 1];
} render_state_t;

typedef enum key_t
{
    KEY_NONE = 0, // preemptively add an extra none-value to account for non-key/analogue inputs
    KEY_1 = 1,
    KEY_2 = 2,
    KEY_3 = 3,
    KEY_4 = 4,
    KEY_5 = 5,
    KEY_6 = 6,
    KEY_7 = 7,
    KEY_8 = 8,
    KEY_9 = 9,
    KEY_QUIT = 10,
    KEY_MAX_KEYS,
    // All unknown keys are registered after the "MAX_KEYS" value
} key_t;

typedef struct input_t
{
    bool has_input;
    key_t key;
} input_t;

#define GET_PUZZLE_HEIGHT(puzzle) ( ARRAYSIZE((puzzle)->puzzleSegments) )
#define GET_PUZZLE_WIDTH(puzzle) ( ARRAYSIZE((puzzle)->puzzleSegments[0]) )

static
void
GetPuzzleSegmentPosition(
    const puzzle_t* puzzle,
    const puzzle_segment_t* puzzleSegment,
    _Out_ size_t* row,
    _Out_ size_t* col)
{
    assert(puzzleSegment >= &puzzle->puzzleSegments[0][0]);
    assert(puzzleSegment <= &puzzle->puzzleSegments[GET_PUZZLE_HEIGHT(puzzle)-1][GET_PUZZLE_WIDTH(puzzle)-1]);

    size_t index = puzzleSegment - &puzzle->puzzleSegments[0][0];
    *row = index / GET_PUZZLE_WIDTH(puzzle);
    *col = index % GET_PUZZLE_WIDTH(puzzle);
}

static
puzzle_segment_t*
FindEmptyTile(
    puzzle_t* puzzle)
{
    for (size_t tile_row = 0; tile_row < GET_PUZZLE_HEIGHT(puzzle); tile_row += 1)
    {
        for (size_t tile_col = 0; tile_col < GET_PUZZLE_WIDTH(puzzle); tile_col += 1)
        {
            puzzle_segment_t* nextSegment = &puzzle->puzzleSegments[tile_row][tile_col];
            if (!nextSegment->isSet)
            {
                return nextSegment;
            }
        }
    }

    assert(false && "Invalid Puzzle! Has no empty segments");
    exit(1);
}

static
void
GetNeighbors(
    puzzle_t* puzzleState,
    puzzle_segment_t* homeTile,
    _Out_ size_t* outNeighborCount,
    _Out_writes_(*outNeighborCount) puzzle_segment_t** outNeighbors)
{
    // Cast our inputs as sized-types to work more naturally with this function
    size_t homeRow;
    size_t homeCol;
    GetPuzzleSegmentPosition(puzzleState, homeTile, &homeRow, &homeCol);

    int row = (int)homeRow;
    int col = (int)homeCol;

    typedef struct neighbor_offset_t
    {
        int8_t row;
        int8_t col;
    } neighbor_offset_t;

    static const neighbor_offset_t neighbors[] =
    {
        { -1,  0 }, // top
        {  0, -1 }, // left
        {  0,  1 }, // right
        {  1,  0 }, // bottom
    };

    static const int PUZZLE_HEIGHT = GET_PUZZLE_HEIGHT(puzzleState);
    static const int PUZZLE_WIDTH = GET_PUZZLE_WIDTH(puzzleState);

    size_t neighborCount = 0;
    for (size_t i = 0; i < ARRAYSIZE(neighbors); i += 1)
    {
        neighbor_offset_t neighborOffset = neighbors[i];
        int offsetRow = row + neighborOffset.row;
        int offsetCol = col + neighborOffset.col;

        // out of bounds neighbor check
        if (offsetRow < 0 || offsetRow >= PUZZLE_HEIGHT ||
            offsetCol < 0 || offsetCol >= PUZZLE_WIDTH)
        {
            continue;
        }

        outNeighbors[neighborCount] = &puzzleState->puzzleSegments[offsetRow][offsetCol];
        neighborCount += 1;
    }

    *outNeighborCount = neighborCount;
}

static
puzzle_segment_t*
FindEmptyNeighbor(
    puzzle_t* puzzleState,
    size_t selectedRow,
    size_t selectedCol)
{
    size_t neighborCount;
    puzzle_segment_t* neighbors[MAX_NEIGHBORS];
    GetNeighbors(puzzleState, &puzzleState->puzzleSegments[selectedRow][selectedCol], &neighborCount, neighbors);

    for (size_t i = 0; i < neighborCount; i += 1)
    {
        if (!neighbors[i]->isSet)
        {
            return neighbors[i];
        }
    }
    return NULL;
}

static
void
InitializeRenderState(
    _Out_ render_state_t* renderState
    )
{
    renderState->lastRenderedState = (game_state_id_t){0};

    static const char CLEAR_CHAR = ' ';

    // Setup the debug clear line
    memset(g_debugRenderState.clearLine, CLEAR_CHAR, DEBUG_LINE_WIDTH);
    g_debugRenderState.clearLine[DEBUG_LINE_WIDTH] = '\n';
    g_debugRenderState.clearLine[DEBUG_LINE_WIDTH + 1] = '\0';
    // Setup the debug backup line
    (void)strcpy_s(g_debugRenderState.backupLine, sizeof(g_debugRenderState.backupLine), CURSORUP_SEQ "\r");

    // Setup the clearframe
    for (size_t row = 0; row < FRAME_HEIGHT; row += 1)
    {
        for (size_t col = 0; col < FRAME_WIDTH; col += 1)
        {
            set_frame_buffer(&renderState->clearFrame, row, col, CLEAR_CHAR);
        }
        set_frame_buffer(&renderState->clearFrame, row, FRAME_WIDTH, '\n');
    }
    renderState->clearFrame[sizeof(renderState->clearFrame) - 1] = '\0';

    // setup the backup frame
    char_span_t offsetBackupFrame = {
        .data = renderState->backupFrame,
        .count = sizeof(renderState->backupFrame) };

    for (size_t row = 0; row < FRAME_HEIGHT; row += 1)
    {
        memcpy(offsetBackupFrame.data, CURSORUP_SEQ, CURSORUP_SEQ_SIZE);
        SPAN_ADV(offsetBackupFrame, CURSORUP_SEQ_SIZE);
    }

    offsetBackupFrame.data[0] = '\r';
    SPAN_ADV(offsetBackupFrame, 1);

    offsetBackupFrame.data[0]= '\0';
    SPAN_ADV(offsetBackupFrame, 1);
    assert(offsetBackupFrame.count == 0);

    // Write an empty frame to the console so that the Render loop doesn't have to special case the first write
    printf("%s%s", g_debugRenderState.clearLine, renderState->clearFrame);
}

static
void
InitializeGame(
    const puzzle_t* puzzle,
    _Out_ game_state_t* gameState)
{
    // TODO(scottnm): hardcoding a single puzzle for now
    *gameState = (game_state_t) {
        // FIXME: how do I make this state offset thing less fragile
        // Start the GameState off at State1 so that it's different from the initial render state
        .stateId = { .value = 1 },
        .gameOverState = GAME_OVER_STATE_NONE,
        .selectedCell = {0, 0}, // always start the cursor in the top-left corner
        .puzzle = {0}, // zero-init the puzzle segments. We'll fill in below.
    };

    // Copy the input puzzle in as both the solution and the initial puzzle state
    memcpy(&gameState->puzzle, puzzle, sizeof(gameState->puzzle));
    memcpy(&gameState->puzzleSolution, puzzle, sizeof(gameState->puzzle));

    // Mix up the initial puzzle state
    puzzle_segment_t* emptyTile = FindEmptyTile(&gameState->puzzle);

    // FIXME: I shouldn't use srand or rand
    srand(0); // use a deterministic seed for now

    // FIXME: increase once I test the win-state logic
    static const size_t MIX_UP_STEPS = 10;
    for (size_t i = 0; i < MIX_UP_STEPS; i += 1)
    {
        size_t neighborCount;
        puzzle_segment_t* neighbors[MAX_NEIGHBORS];
        GetNeighbors(&gameState->puzzle, emptyTile, &neighborCount, neighbors);

        // select a random neighbor cell
        puzzle_segment_t* swapTile = neighbors[rand() % neighborCount];
        // swap
        SwapPuzzleSegments(emptyTile, swapTile);

        emptyTile = swapTile;
    }
}

static
bool
GameRunning(
    const game_state_t* gameState)
{
    assert(gameState != NULL);
    return gameState->gameOverState == GAME_OVER_STATE_NONE;
}

static
input_t
PollInput()
{
    raw_key_t polled_key = get_raw_key();
    if (!polled_key.hit)
    {
        return (input_t){0};
    }

    key_t key;
    switch (polled_key.keyCode)
    {
        case '1':
            key = KEY_1;
            break;
        case '2':
            key = KEY_2;
            break;
        case '3':
            key = KEY_3;
            break;
        case '4':
            key = KEY_4;
            break;
        case '5':
            key = KEY_5;
            break;
        case '6':
            key = KEY_6;
            break;
        case '7':
            key = KEY_7;
            break;
        case '8':
            key = KEY_8;
            break;
        case '9':
            key = KEY_9;
            break;
        case 'q':
        case 'Q':
        case RAW_KEY_ESC:
            key = KEY_QUIT;
            break;
        default:
            key = (key_t)(KEY_MAX_KEYS + polled_key.keyCode);
            WRITE_DEBUG_LINE("Unknown key! 0x%08X (keyCode=0x%08X)", key, polled_key.keyCode);
            break;
    }

    return (input_t){ .has_input = true, .key = key };
}

static
void
UpdateGameState(
    input_t input,
    _Inout_ game_state_t* gameState)
{
    if (!input.has_input)
    {
        return;
    }

    if (input.key == KEY_QUIT)
    {
        gameState->gameOverState = GAME_OVER_STATE_QUIT;
        return;
    }

    // Check if we've selected a tile to move
    if (input.key >= KEY_1 && input.key <= KEY_9)
    {
        size_t keyIndex = input.key - KEY_1; // shift the key to a 0-based index
        size_t selectedRow = keyIndex / GRID_DIMENSION;
        size_t selectedCol = keyIndex % GRID_DIMENSION;
        puzzle_segment_t* selectedTile = &gameState->puzzle.puzzleSegments[selectedRow][selectedCol];
        if (!selectedTile->isSet)
        {
            // noop input. selected an already empty tile so there is no move
            WRITE_DEBUG_LINE("Selected tile at (%zu,%zu) is the empty tile", selectedRow, selectedCol);
            return;
        }

        // FIXME: rather than search for the empty neighbor I could always cache it/store it.
        // Given the size of this problem and the fact that it's a prototype idk that I care right now.
        puzzle_segment_t* emptyTile = FindEmptyNeighbor(&gameState->puzzle, selectedRow, selectedCol);
        if (emptyTile == NULL)
        {
            // noop input. the selected tile was not a neighbor of the empty tile
            WRITE_DEBUG_LINE("Tile at (%zu,%zu) not neighbor of empty tile", selectedRow, selectedCol);
            return;
        }

        // Swap the empty tile with the selected tile
        size_t emptyTileRow, emptyTileCol;
        GetPuzzleSegmentPosition(&gameState->puzzle, emptyTile, &emptyTileRow, &emptyTileCol);
        WRITE_DEBUG_LINE("Swapping (%zu,%zu) with (%zu,%zu)", selectedRow, selectedCol, emptyTileRow, emptyTileCol);

        SwapPuzzleSegments(emptyTile, selectedTile);
    }

    // The board has updated, check to see if we've won!

    // N.B. memcmp is probably not the most robust way to compare board states but it sure is simple.
    // The usual concern about memcmp not handling padding bytes well doesn't apply since we the puzzle state and the
    // final puzzle solution have any/all padding bytes copied around appropriately.
    // FIXME: where we are doing raw copies look for places where we should static assert size constraints
    bool hasWon = mem_eq(&gameState->puzzle, &gameState->puzzleSolution, sizeof(gameState->puzzle));
    if (hasWon)
    {
        gameState->gameOverState = GAME_OVER_STATE_WON;
    }

    // The state has updated, bump the state id so that it's rendered in the next frame.
    gameState->stateId.value += 1;
}

static
void
Render(
    const game_state_t* gameState,
    _Inout_ render_state_t* renderState)
{
    if (gameState->stateId.value == renderState->lastRenderedState.value && !g_debugRenderState.dirty)
    {
        // if nothing has changed in the gamestate, don't bother rendering an update
        return;
    }

    // Clear last frame
    printf("%s%s%s%s%s%s",
        renderState->backupFrame,
        g_debugRenderState.backupLine,
        g_debugRenderState.clearLine,
        renderState->clearFrame,
        renderState->backupFrame,
        g_debugRenderState.backupLine);

    // Render the debug line
    printf("%s\n", g_debugRenderState.debugLine);

    // Render the next frame
    char renderFrame[FRAME_BUFFER_SIZE];
    // FIXME: static_assert on the size
    memcpy(renderFrame, renderState->clearFrame, sizeof(renderState->clearFrame));

    for (size_t tile_row = 0; tile_row < GRID_DIMENSION; tile_row += 1)
    {
        for (size_t tile_col = 0; tile_col < GRID_DIMENSION; tile_col += 1)
        {
            // Skip any unset segments
            const puzzle_segment_t* nextPuzzleSegment = &gameState->puzzle.puzzleSegments[tile_row][tile_col];
            if (!nextPuzzleSegment->isSet)
            {
                continue;
            }

            for (size_t tile_cell_row = 0; tile_cell_row < TILE_DIMENSION; tile_cell_row += 1)
            {
                for (size_t tile_cell_col = 0; tile_cell_col < TILE_DIMENSION; tile_cell_col += 1)
                {
                    size_t frame_cell_row = ((TILE_DIMENSION + 1) * tile_row) + 1 + tile_cell_row;
                    size_t frame_cell_col = ((TILE_DIMENSION + 1) * tile_col) + 1 + tile_cell_col;
                    set_frame_buffer(
                        &renderFrame,
                        frame_cell_row,
                        frame_cell_col,
                        nextPuzzleSegment->imgData[tile_cell_row][tile_cell_col]);
                }
            }
        }
    }

    printf("%s", renderFrame);
    renderState->lastRenderedState = gameState->stateId;
    g_debugRenderState.dirty = false;
}

static
void
ReadPuzzle(
    str_t puzzleFilePath,
    _Out_ puzzle_t* outPuzzle
    )
{
    FILE* puzzleFile;
    errno_t err = fopen_s(&puzzleFile, puzzleFilePath.bytes, "rb");
    if (err != 0)
    {
        // FIXME: for now don't worry about proper error handling. Just force an exit here.
        // This is just a prototype.
        Log("Failed to open puzzle file, %s! err=0x%08X", puzzleFilePath.bytes, err);
        exit(err);
    }

    err = fseek(puzzleFile, 0, SEEK_END);
    assert(err == 0);

    size_t puzzleFileByteCount = ftell(puzzleFile);
    err = fseek(puzzleFile, 0, SEEK_SET);
    assert(err == 0);

    static char puzzleFileByteBuffer[
        (GRID_DIMENSION * GRID_DIMENSION) * // MAX_PUZZLE_FILE_SEGMENTS *
        (ARRAYSIZE("256,256\n") - 1 + sizeof(puzzle_segment_t)) // MAX_SIZE_OF_EACH_DATA_SEGMENT
    ];

    if (puzzleFileByteCount > sizeof(puzzleFileByteBuffer))
    {
        // FIXME: for now don't worry about proper error handling. Just force an exit here.
        // This is just a prototype.
        Log("Error parsing puzzle data! expected at most %zu bytes but found %zu bytes",
            sizeof(puzzleFileByteBuffer), puzzleFileByteCount);
        exit(1);
    }

    size_t bytesRead = fread(puzzleFileByteBuffer, 1, sizeof(puzzleFileByteBuffer), puzzleFile);
    char_span_t puzzleFileBytes = {
        .data = puzzleFileByteBuffer,
        .count = bytesRead,
    };

    char_span_t nextPuzzleFileLine = { .data = puzzleFileByteBuffer, .count = 0 };
    puzzle_t puzzleBuffer = {0};
    while (true)
    {
        nextPuzzleFileLine = get_next_split(nextPuzzleFileLine, puzzleFileBytes, '\n');
        if (nextPuzzleFileLine.data == NULL)
        {
            break;
        }

        char* splitPtr;
        size_t row = strtoul(nextPuzzleFileLine.data, &splitPtr, 10);
        if (*splitPtr != ',')
        {
            // FIXME: for now don't worry about proper error handling. Just force an exit here.
            // This is just a prototype.
            Log("Error parsing puzzle data! expected comma after row but found %c(%i)", *splitPtr, *splitPtr);
            exit(1);
        }

        size_t col = strtoul(splitPtr + 1, &splitPtr, 10);
        if (*splitPtr != '\n')
        {
            // FIXME: for now don't worry about proper error handling. Just force an exit here.
            // This is just a prototype.
            Log("Error parsing puzzle data! expected newline after col but found %c(%i)", *splitPtr, *splitPtr);
            exit(1);
        }

        if (row >= GRID_DIMENSION)
        {
            // FIXME: for now don't worry about proper error handling. Just force an exit here.
            // This is just a prototype.
            Log("Error parsing puzzle data! row value (%zu) out of range [0,%u)", row, GRID_DIMENSION);
            exit(1);
        }

        if (col >= GRID_DIMENSION)
        {
            // FIXME: for now don't worry about proper error handling. Just force an exit here.
            // This is just a prototype.
            Log("Error parsing puzzle data! col value (%zu) out of range [0,%u)", col, GRID_DIMENSION);
            exit(1);
        }

        puzzle_segment_t* puzzleSegment = &puzzleBuffer.puzzleSegments[row][col];
        if (puzzleSegment->isSet)
        {
            Log("Error parsing puzzle data! puzzle segment (%zu,%zu) assigned twice", row, col);
            exit(1);
        }

        puzzleSegment->isSet = true;
        for (size_t i = 0; i < ARRAYSIZE(puzzleSegment->imgData); ++i)
        {
            nextPuzzleFileLine = get_next_split(nextPuzzleFileLine, puzzleFileBytes, '\n');
            if (nextPuzzleFileLine.data == NULL)
            {
                Log("Error parsing puzzle data! each puzzle segment needs at least %zu rows but failed to find row %zu",
                    ARRAYSIZE(puzzleSegment->imgData), i);
                exit(1);
            }

            if (sizeof(puzzleSegment->imgData[i]) != nextPuzzleFileLine.count)
            {
                Log("Error parsing puzzle data! expected %zu bytes in puzzle segment row but found %zu",
                    sizeof(puzzleSegment->imgData[i]), nextPuzzleFileLine.count);
                exit(1);
            }

            memcpy(puzzleSegment->imgData[i], nextPuzzleFileLine.data, sizeof(puzzleSegment->imgData[i]));
        }
    }

    size_t totalSetSegmentCount = 0;
    for (size_t row = 0; row < ARRAYSIZE(puzzleBuffer.puzzleSegments); row += 1)
    {
        for (size_t col = 0; col < ARRAYSIZE(puzzleBuffer.puzzleSegments[row]); col += 1)
        {
            if (puzzleBuffer.puzzleSegments[row][col].isSet)
            {
                totalSetSegmentCount += 1;
            }
        }
    }

    const size_t TOTAL_PUZZLE_SEGMENT_COUNT =
        ARRAYSIZE(puzzleBuffer.puzzleSegments) * ARRAYSIZE(puzzleBuffer.puzzleSegments[0]);
    const size_t EXPECTED_PUZZLE_SEGMENT_COUNT = TOTAL_PUZZLE_SEGMENT_COUNT - 1;
    if (totalSetSegmentCount != EXPECTED_PUZZLE_SEGMENT_COUNT)
    {
        Log("Error parsing puzzle data! expected %zu complete puzzle segments but found %zu",
            EXPECTED_PUZZLE_SEGMENT_COUNT, totalSetSegmentCount);
        exit(1);
    }

    *outPuzzle = puzzleBuffer;
}

void
main(
    void)
{
    str_t puzzleFilePath = cstr("data/puzzle.data");
    puzzle_t puzzle;
    ReadPuzzle(puzzleFilePath, &puzzle);

    render_state_t renderState = {0};
    InitializeRenderState(&renderState);

    game_state_t gameState = {0};
    InitializeGame(&puzzle, &gameState);

    while (GameRunning(&gameState))
    {
        input_t input = PollInput();
        UpdateGameState(input, &gameState);
        Render(&gameState, &renderState);
        Sleep(SLEEP_PERIOD_MS);
    }

    switch (gameState.gameOverState)
    {
        case GAME_OVER_STATE_WON:
            Log("Won!");
            break;
        case GAME_OVER_STATE_QUIT:
            Log("Quitting...");
            break;
        default:
            assert(false && "Invalid GameOver state!");
            break;
    }
}