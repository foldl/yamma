
#ifndef _kernel_h
#define _kernel_h

#define KVERSION 1

#define MOK             0
#define MERROR          1

#define ioSTD           0
#define ioERR           1
#define ioMSG           2
#define ioSND           3
#define ioIMG           4
#define ioDAT           5

typedef int (*f_io_input)(int port, 
                          char *p, const int size,
                          const char *prompt, const int prompt_len,
                          const char *init, const int init_len);
typedef int (*f_io_output)(int port, 
                          const char *p, const int size);

struct outside
{
    int version;

    // io
    //
    f_io_input io_input;
    f_io_output io_output;
    const char *encoding;
    const char *lang; // "mma", "lisp"

    // datetime
    //

    // file system
    //

    // thread
    //

    // dll
};

int Kernel_Init(outside *p);
int Kernel_Eval(const char *p, const int size, const bool bPrint = true);

#endif
