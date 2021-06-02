#include "pch.h"

typedef struct game_state_t
{
    uint8_t dummy_byte;
} game_state_t;

typedef struct render_state_t
{
    uint8_t dummy_byte;
} render_state_t;

typedef struct input_t
{
    uint8_t dummy_byte;
} input_t;

void
InitializeGame(
    _Out_ game_state_t* gameState)
{
    UNREF(gameState);
    TODO("Need to initialize the game state");
}

bool
GameRunning(
    const game_state_t* gameState)
{
    UNREF(gameState);
    TODO("Need to check that game is still running");
    return true;
}

input_t
PollInput()
{
    TODO("Need to poll for input");
    return (input_t){0};
}

void
UpdateGameState(
    input_t input,
    _Inout_ game_state_t* gameState)
{
    UNREF(input);
    UNREF(gameState);
    TODO("Need to update the game state!");
}

void
Render(
    const game_state_t* gameState,
    _Inout_ render_state_t* renderState)
{
    UNREF(gameState);
    UNREF(renderState);
    TODO("Need to actually render!");
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
        UpdateGameState(input, &gameState);
        Render(&gameState, &renderState);
    }
}