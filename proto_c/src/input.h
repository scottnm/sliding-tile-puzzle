// Returns an key code
typedef struct raw_key_t
{
    bool hit;
    int keyCode;
} raw_key_t;

raw_key_t
get_raw_key();
