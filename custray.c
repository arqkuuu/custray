#include <Imlib2.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define isanum(str) (strtol((str), NULL, 10) != 0)

typedef struct {
    int number;
    char *btn_exec;
} btn_exec;

btn_exec *btn_execs = NULL;
int btn_execs_size = 0;
char *icon = NULL;
int size = 20;

void cleanup() {
    for (int i = 0; i < btn_execs_size; i++) free(btn_execs[i].btn_exec);
    free(btn_execs);
    if (icon != NULL) imlib_free_image();
    exit(0);
}

void help() {
    printf(
        "\n-[1...∞] [any oneliner] - assign a command to the mouse button.\n"
        "1 should be your left button, 2 the middle, and 3 the right.\n"
        "You can assign commands to as many buttons, as you have on your mouse.\n"
        "Spinning your wheel is considered a button too. One for each direction.\n\n"
        "-i [path] - path to the icon image\n"
        "-s [number] - desired size of the icon\n"
        "-h - show this help\n"
        "\nusage example:\n\n"
        "custray -i /home/arqkuuu/icon.jpg -1 mpc toggle -3 /home/arqkuuu/scripts/mpd_xmenu.sh -s 24\n\n");
    cleanup();
}

void parse_args(char **argv, int argc) {
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h")) {
                help();
            } else if (!strcmp(argv[i], "-i")) {
                if ((i + 1) < argc && argv[i + 1][0] != '-' && access(argv[i + 1], R_OK) == 0) icon = argv[i+1];
                else printf("Illegal icon path. Skipping the argument.\n");
            } else if (!strcmp(argv[i], "-s")) {
                if ((i + 1) < argc && isanum(argv[i + 1])) size = atoi(argv[i + 1]);
                else printf("Illegal size. Skipping the argument.\n");
            } else if (isanum((argv[i] + 1))) {
                btn_execs = (btn_exec *)realloc(btn_execs, (btn_execs_size + 1) * sizeof(btn_exec));
                btn_execs[btn_execs_size].number = atoi(argv[i] + 1);
                btn_execs[btn_execs_size].btn_exec = (char *)malloc(sizeof(char));
                *btn_execs[btn_execs_size].btn_exec = '\0';
                for (int j = i + 1; j < argc; j++) {
                    if (argv[j][0] == '-') break;
                    btn_execs[btn_execs_size].btn_exec = (char *)realloc(btn_execs[btn_execs_size].btn_exec, (strlen(btn_execs[btn_execs_size].btn_exec) + strlen(argv[j]) + 1) * sizeof(char));
                    sprintf(btn_execs[btn_execs_size].btn_exec, "%s ", argv[j]);
                }
                btn_execs_size++;
            }
        }
    }
}

void set_context_icon(Display *dpy, XVisualInfo vinfo, XSetWindowAttributes attr, Window win) {
    imlib_context_set_display(dpy);
    imlib_context_set_visual(vinfo.visual);
    imlib_context_set_colormap(attr.colormap);
    imlib_context_set_drawable(win);
}

void resize_icon(Imlib_Image image) {
    Imlib_Image res = imlib_create_cropped_scaled_image(0, 0, imlib_image_get_width(), imlib_image_get_height(), size, size);
    imlib_free_image();
    imlib_context_set_image(res);
}

void load_icon() {
    Imlib_Image image = imlib_load_image(icon);
    imlib_context_set_image(image);
    resize_icon(image);
}

void rerender_icon() {
    XClearWindow(imlib_context_get_display(), imlib_context_get_drawable());
    imlib_render_image_on_drawable(0, 0);
}

Window get_tray(Display *dpy) {
    Atom selection_atom = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
    Window tray = XGetSelectionOwner(dpy, selection_atom);
    if (tray != None) XSelectInput(dpy, tray, SubstructureNotifyMask);
    return tray;
}

void send_to_tray(Display *dpy, Window tray, long message, long data1, long data2, long data3) {
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray;
    ev.xclient.message_type = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = message;
    ev.xclient.data.l[2] = data1;
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;
    XSendEvent(dpy, tray, False, NoEventMask, &ev);
    XSync(dpy, False);
    usleep(10000);
}

Window create_window(Display *dpy, Window root) {
    Window win;
    XSetWindowAttributes attr;
    if (icon != NULL) {
        XVisualInfo vinfo;
        XMatchVisualInfo(dpy, DefaultScreen(dpy), 32, TrueColor, &vinfo);
        attr.colormap = XCreateColormap(dpy, DefaultRootWindow(dpy), vinfo.visual, AllocNone);
        attr.border_pixel = 0;
        attr.background_pixel = 0;
        win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, size, size, 0, vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel, &attr);
        set_context_icon(dpy, vinfo, attr, win);
    } else {
        srand((unsigned int)time(NULL));
        printf("No icon path! Defaulting to a random color.\n");
        win = XCreateSimpleWindow(dpy, root, 0, 0, size, size, 0, 0, rand());
    }
    attr.override_redirect = True;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attr);
    return win;
}

void start_mainloop(Display *dpy, Window win) {
    XFixesSelectSelectionInput(dpy, DefaultRootWindow(dpy), XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False), XFixesSetSelectionOwnerNotifyMask);
    XSelectInput(dpy, win, ButtonPressMask);
    int evbase, errbase;
    int xfix = XFixesQueryExtension(dpy, &evbase, &errbase);
    assert(xfix);
    XEvent ev;
    Window tray;
    tray = get_tray(dpy);
    if (tray != None)send_to_tray(dpy, tray, 0, win, 0, 0);
    if (icon != NULL) load_icon();
    XSync(dpy, False);
    for (;;) {
        XNextEvent(dpy, &ev);
        if (ev.type == ButtonPress) {
            for (int i = 0; i < btn_execs_size; i++) {
                if (ev.xbutton.button == btn_execs[i].number) {
                    system(btn_execs[i].btn_exec);
                }
            }
        } else if (ev.type == (evbase + XFixesSelectionNotify)) {
            tray = get_tray(dpy);
            if (tray != None) {
                XMapWindow(dpy, win);
                send_to_tray(dpy, tray, 0, win, 0, 0);
            } else {
                XUnmapWindow(dpy, win);
            }
        } else if (icon != NULL) rerender_icon();
        XFlush(dpy);
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, cleanup);
    signal(SIGABRT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGTSTP, cleanup);
    parse_args(argv, argc);
    Display *dpy;
    int screen;
    Window root, win;
    if (!(dpy = XOpenDisplay(NULL))) return -1;
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    win = create_window(dpy, root);
    start_mainloop(dpy, win);
    return 1;
}
