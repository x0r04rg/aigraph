
#define INPUT_SIZE 256

struct console
{
    char input[INPUT_SIZE];
    char **history;
    int history_size, history_capacity;
    int hidden;
};

static char*
skip_whitespace(char *src)
{
    char *p = src;
    while (*p == ' ') ++p;
    return p;
}

static char* 
read_word(char *src, char *buf, int buf_size)
{
    int i = 0, n = buf_size - 1;
    char *p = skip_whitespace(src);
    while (*p != '\0' && *p != ' ' && i < n) buf[i++] = *p++;
    buf[i] = '\0';
    return p;
}

static int
count_words(char *src)
{
    char buf[INPUT_SIZE];
    char *p = src;
    int i = 0;
    while (*p != '\0') { p = read_word(p, buf, NK_LEN(buf)); ++i; }
    return i;
}

static void
console_init(struct console *console)
{
    memset(console, 0, sizeof *console);
    console->hidden = nk_true;
}

static void
console_print(struct console *console, char *string)
{
    if (console->history_size == console->history_capacity)
    {
        int new_capacity = console->history_capacity ? 2 * console->history_capacity : 10;
        console->history = realloc(console->history, new_capacity * sizeof *console->history);
        console->history_capacity = new_capacity;
    }
    console->history[console->history_size++] = _strdup(string);
}

static void console_printfv(struct console *console, char *fmt, va_list args)
{
    char buf[INPUT_SIZE];
    _vsnprintf(buf, INPUT_SIZE, fmt, args);
    console_print(console, buf);
}

static void 
console_printf(struct console *console, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_printfv(console, fmt, args);
    va_end(args); 
}

static void 
console_execute(struct console *console, struct node_editor *editor, char *string)
{
#define ARGCHECK(cmd, cond) do { if (!(cond)) { console_print(console, "error: wrong number of arguments for '%s' command", cmd); return; } } while(0)

    char buf[INPUT_SIZE];
    char *p = string;
    int argc;

    if (!p) return;

    p = read_word(p, buf, NK_LEN(buf));

    if (buf[0] == '\0') return;

    console_print(console, skip_whitespace(string));
    p = skip_whitespace(p);
    argc = count_words(p);

    if (!strcmp(buf, "save"))
    {
        ARGCHECK("save", argc == 1);
        sprintf_s(buf, NK_LEN(buf), "%s.aig", p);
        node_editor_save(editor, buf);
    }
    else if (!strcmp(buf, "load"))
    {
        ARGCHECK("load", argc == 1);
        sprintf_s(buf, NK_LEN(buf), "%s.aig", p);
        node_editor_load(editor, buf);
    }
    else
    {
        console_print(console, "error: invalid command");
    }

#undef ARGCHECK
}

static void 
console_gui(struct nk_context *ctx, struct console *console, struct node_editor *editor, 
    struct nk_rect window)
{
    nk_flags fl = 0;
    int just_shown = 0;
    int scroll_down = 0;

    if (console->hidden != nk_window_is_hidden(ctx, "console"))
    {
        if (!console->hidden)
        {
            just_shown = 1;
            nk_window_show(ctx, "console", NK_SHOWN);
        }
        else
        {
            struct nk_window *window = nk_window_find(ctx, "console");
            if (window) window->edit.active = nk_false;
            nk_window_show(ctx, "console", NK_HIDDEN);
        }
    }

    if (!console->hidden) nk_window_set_focus(ctx, "console");
    else fl |= NK_WINDOW_HIDDEN;

    window.h *= 0.4f;
    if (nk_begin(ctx, "console", window, NK_WINDOW_SCROLL_AUTO_HIDE | fl))
    {
        struct nk_window *w = ctx->current;

        /* draw history */
        for (int i = 0; i < console->history_size; ++i)
        {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, console->history[i], NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
        }

        /* draw padding if needed */
        struct nk_rect c = nk_window_get_content_region(ctx);
        struct nk_rect b = nk_layout_space_bounds(ctx);
        float pad = c.h - (console->history_size + 1) * 29;
        if (pad > 0)
        {
            nk_layout_row_dynamic(ctx, pad - 10, 1);
            nk_label(ctx, "", 0);
        }

        /* draw input */
        if (!just_shown) nk_edit_focus(ctx, 0);
        nk_layout_row_dynamic(ctx, 25, 1);
        nk_flags res = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER | NK_EDIT_AUTO_SELECT, 
            console->input, INPUT_SIZE, nk_filter_ascii);
        if (res & NK_EDIT_COMMITED)
        {
            console_execute(console, editor, console->input);
            console->input[0] = '\0';
            scroll_down = 1;
        }
    }
    nk_end(ctx);

    if (scroll_down || just_shown)
    {
        struct nk_window *w = nk_window_find(ctx, "console");
        if (w->layout)
        {
            float offset = (console->history_size + 1) * 29 + 4 - w->layout->bounds.h;
            if (offset > 0) *w->layout->offset_y = offset;
        }
    }
}