#include "pch.h"

#define SLEEP_PERIOD_MS 30

// 3X3 slide puzzle where each slide piece is 5x5 with a 1 cell buffer around each cell
// width == (bufwidth + cell_width) * num_cells + bufwidth == (1+5)*3 + 1 == 19
// height == width (it's square)
#define FRAME_WIDTH 19
#define FRAME_HEIGHT 19
//   width # of clear cells + a newline for each row
// + 1 for the null terminator
#define FRAME_BUFFER_SIZE (((FRAME_WIDTH + 1) * FRAME_HEIGHT) + 1)

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

typedef struct game_state_t
{
    game_state_id_t stateId;
    bool isRunning;
} game_state_t;

typedef struct render_state_t
{
    game_state_id_t lastRenderedState;
    char clearFrame[FRAME_BUFFER_SIZE];

    //   1 cursor-up character for each line +
    // + 1 backup character once we've reached the top of the line
    // + 1 for the null terminator
#define BACKUP_SEQ "\033[A"
#define BACKUP_SEQ_SIZE (sizeof(BACKUP_SEQ)-1)
    char backupFrame[(FRAME_HEIGHT * BACKUP_SEQ_SIZE) + 1 + 1];
} render_state_t;

typedef enum key_t
{
    KEY_NONE = 0, // preemptively add an extra none-value to account for non-key/analogue inputs
    KEY_QUIT = 1,
    KEY_MAX_KEYS,
    // All unknown keys are registered after the "MAX_KEYS" value
} key_t;

typedef struct input_t
{
    bool has_input;
    key_t key;
} input_t;

void
InitializeRenderState(
    _Out_ render_state_t* renderState
    )
{
    renderState->lastRenderedState = (game_state_id_t){0};

    // Setup the clearframe
    static const char CLEAR_CHAR = ' ';
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
        memcpy(offsetBackupFrame.data, BACKUP_SEQ, BACKUP_SEQ_SIZE);
        SPAN_ADV(offsetBackupFrame, BACKUP_SEQ_SIZE);
    }

    offsetBackupFrame.data[0] = '\r';
    SPAN_ADV(offsetBackupFrame, 1);

    offsetBackupFrame.data[0]= '\0';
    SPAN_ADV(offsetBackupFrame, 1);
    assert(offsetBackupFrame.count == 0);
}

void
InitializeGame(
    _Out_ game_state_t* gameState)
{
    // FIXME: how do I make this state offset thing less fragile
    // Start the GameState off at State1 so that it's different from the initial render state
    *gameState = (game_state_t) { .stateId = { .value = 1 }, .isRunning = true };
}

bool
GameRunning(
    const game_state_t* gameState)
{
    assert(gameState != NULL);
    return gameState->isRunning;
}

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
        case 'q':
        case 'Q':
        case RAW_KEY_ESC:
            key = KEY_QUIT;
            break;
        default:
            key = (key_t)(KEY_MAX_KEYS + polled_key.keyCode);
            Log("Unknown key! 0x%08X (keyCode=0x%08X)", key, polled_key.keyCode);
            break;
    }

    return (input_t){ .has_input = true, .key = key };
}

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
        Log("Quit key pressed! Quitting...");
        gameState->isRunning = false;
        return;
    }
}

void
Render(
    const game_state_t* gameState,
    _Inout_ render_state_t* renderState)
{
    if (gameState->stateId.value == renderState->lastRenderedState.value)
    {
        // if nothing has changed in the gamestate, don't bother rendering an update
        return;
    }

    // Clear last frame
    printf("%s%s", renderState->clearFrame, renderState->backupFrame);

    // Render the next frame
    char renderFrame[FRAME_BUFFER_SIZE];
    // FIXME: static_assert on the size
    memcpy(renderFrame, renderState->clearFrame, sizeof(renderState->clearFrame));

    const int GRID_DIMENSION = 3; // 3x3 grid of tiles
    const int TILE_DIMENSION = 5; // each tile is 5x5 set of cells
    const char DUMMY_CELL_CHAR = 'X';

    for (size_t tile_row = 0; tile_row < GRID_DIMENSION; tile_row += 1)
    {
        for (size_t tile_col = 0; tile_col < GRID_DIMENSION; tile_col += 1)
        {
            for (size_t tile_cell_row = 0; tile_cell_row < TILE_DIMENSION; tile_cell_row += 1)
            {
                for (size_t tile_cell_col = 0; tile_cell_col < TILE_DIMENSION; tile_cell_col += 1)
                {
                    size_t frame_cell_row = ((TILE_DIMENSION + 1) * tile_row) + 1;
                    size_t frame_cell_col = ((TILE_DIMENSION + 1) * tile_col) + 1;
                    set_frame_buffer(&renderState->clearFrame, frame_cell_row, frame_cell_col, DUMMY_CELL_CHAR);
                }
            }
        }
    }

    printf("%s", renderFrame);
    renderState->lastRenderedState = gameState->stateId;
}

void
main(
    void)
{
    render_state_t renderState = {0};
    InitializeRenderState(&renderState);

    game_state_t gameState = {0};
    InitializeGame(&gameState);

    while (GameRunning(&gameState))
    {
        input_t input = PollInput();
        /* FIXME: TMP */ if (input.has_input) { Log("Input: key=0x%08X", input.key); }
        UpdateGameState(input, &gameState);
        Render(&gameState, &renderState);
        Sleep(33); // 30 FPS
    }
}