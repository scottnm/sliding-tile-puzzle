#include "pch.h"

// FIXME: my style is all kinds of confused. eh do I care? it's a prototype

typedef struct game_state_t
{
    bool isRunning;
} game_state_t;

typedef struct render_state_t
{
    uint8_t dummy_byte;
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
InitializeGame(
    _Out_ game_state_t* gameState)
{
    *gameState = (game_state_t) { .isRunning = true };
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
    // TODO(scottnm): impl
    UNREF(gameState);
    UNREF(renderState);
}

void
main(
    void)
{
    render_state_t renderState = {0};
    game_state_t gameState = {0};

    InitializeGame(&gameState);
    while (GameRunning(&gameState))
    {
        input_t input = PollInput();
        /* FIXME: TMP */ if (input.has_input) { Log("Input: key=0x%08X", input.key); }
        UpdateGameState(input, &gameState);
        Render(&gameState, &renderState);
    }
}