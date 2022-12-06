#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#include "hsl256.h"
#include "rgb256.h"

extern int hsl256[][3];
extern int rgb256[][3];

// if true color is supported, maybe change uint8_t to other larger type
uint8_t* ansi_index = NULL;

typedef enum Distance
{
    DIST_HSL,
    DIST_RGB,
} Distance;

typedef struct arguments
{
    int mode;
    int a;
    int b;
    int c;
    int i;
} arguments;

static void rgb_to_hsl(int r, int g, int b, int *h, int *s, int *l)
{
    float r01 = r/255.0f;
    float g01 = g/255.0f;
    float b01 = b/255.0f;
    float cmax = r01;
    if (g01 > cmax) cmax = g01;
    if (b01 > cmax) cmax = b01;
    float cmin = r01;
    if (g01 < cmin) cmin = g01;
    if (b01 < cmin) cmin = b01;
    float delta = cmax - cmin;
    float epsilon = 1e-6;
    float hf = 0;
    if (delta < epsilon) hf = 0;
    else if (cmax == r01) hf = 60.0f*fmod((g01 - b01)/delta, 6.0f);
    else if (cmax == g01) hf = 60.0f*((b01 - r01)/delta + 2);
    else if (cmax == b01) hf = 60.0f*((r01 - g01)/delta + 4);
    else assert(0 && "unreachable");

    float lf = (cmax + cmin)/2;

    float sf = 0;
    if (delta < epsilon) sf = 0;
    else sf = delta/(1 - fabsf(2*lf - 1));

    *h = fmodf(fmodf(hf, 360.0f) + 360.0f, 360.0f);
    *s = sf*100.0f;
    *l = lf*100.0f;
}

static int distance256(int table256[256][3], int i, int a, int b, int c)
{
    int da = a - table256[i][0];
    int db = b - table256[i][1];
    int dc = c - table256[i][2];
    return da*da + db*db + dc*dc;
}

static int find_ansi_index(int table256[256][3], int a, int b, int c)
{
    int index = 0;
    for (int i = 0; i < 256; ++i) 
    {
        if (distance256(table256, i, a, b, c) < distance256(table256, index, a, b, c)) 
        {
            index = i;
        }
    }
    return index;
}

static void* fill_ansi_index( void* args )
{
    arguments source = *(arguments*) args;
    if ( source.mode == DIST_HSL )
    {
        ansi_index[source.i] = find_ansi_index( hsl256, source.a, source.b, source.c );
    }
    if ( source.mode == DIST_RGB )
    {
        ansi_index[source.i] = find_ansi_index( rgb256, source.a, source.b, source.c );
    }
    return NULL;
}

static char *shift_args(int *argc, char ***argv)
{
    assert(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

static void print_result( int resized_height, int resized_width )
{
    int print_counter = 0;
    for (int y = 0; y < resized_height; ++y) 
    {
        for (int x = 0; x < resized_width; ++x) 
        {
            printf("\e[48;5;%dm  ", ansi_index[ print_counter++ ] );
        }
        printf("\e[0m\n");
    }
    printf("\e[0m\n");

}

void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [OPTIONS...] [FILES...]\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -w      width of the image default is %d\n", 48);
    fprintf(stderr, "    -rgb    search nearest color in RGB space\n");
    fprintf(stderr, "    -hsl    search nearest color in HSL space (default)\n");
    fprintf(stderr, "    -r      ratio of the image according to font ratio\n");
    fprintf(stderr, "    -t      maxium threads used when computing the result\n");
    fprintf(stderr, "    -h      print this help and exit\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "    $ %s -w 16 image1.png -rgb -w 32 image2.png -w 720 -r 0.625 -t 4 image3.png\n", program);
    fprintf(stderr, "    Print image1.png with width 16 and HSL search space then \n");
    fprintf(stderr, "    print image2.png with width 32 and RGB search space then \n");
    fprintf(stderr, "    print image3.png with width 720, height reduced to 0.625x, maxium 4 threads. \n" );
    fprintf(stderr, "    Each flags override the previous one.\n");
}

int main(int argc, char **argv)
{
    // TODO: Add 16 colors support
    // TODO: Add TrueColor support
    assert(argc > 0);
    const char *program = shift_args(&argc, &argv);
    if (argc == 0 )
    {
        usage(program);
        exit(1);
    }

    int resized_width = 48;
    int thread_count = 1;
    float fixed = 1.0f;
    Distance distance = DIST_HSL;

    // TODO: implement help flag that explains the usage
    // TODO: throw an error if not a single file was provided
    while (argc > 0) 
    {
        const char *flag = shift_args(&argc, &argv);
        if (strcmp(flag, "-w") == 0) 
        {
            if (argc <= 0) 
            {
                fprintf(stderr, "ERROR: no value is provided for %s\n", flag);
                exit(1);
            }

            resized_width = atoi(shift_args(&argc, &argv));

            if (resized_width < 0) 
            {
                fprintf(stderr, "ERROR: the value of %s can't be negative\n", flag);
                exit(1);
            }
        }
        else if (strcmp(flag, "-t") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "ERROR: no value is provided for %s\n", flag);
                exit(1);
            }
            thread_count = atoi(shift_args(&argc, &argv));

            if(thread_count < 1)
            {
                fprintf(stderr, "ERROR: the value of %s can't be less than one\n", flag);
                exit(1);
            }
        }
        else if (strcmp(flag, "-r") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "ERROR: no value is provided for %s\n", flag);
                exit(1);
            }
            fixed = atof(shift_args(&argc, &argv));

            if(fixed < 0 )
            {
                fprintf(stderr, "ERROR: the value of %s can't be negative\n", flag);
                exit(1);
            }
        }
        else if (strcmp(flag, "-rgb") == 0) 
        {
            distance = DIST_RGB;
        } 
        else if (strcmp(flag, "-hsl") == 0) 
        {
            distance = DIST_HSL;
        }
        else if (strcmp(flag, "-h") == 0)
        {
            usage(program);
            exit(1);
        }
        else 
        {
            const char *file_path = flag;

            int width, height;
            uint32_t *pixels = (uint32_t*)stbi_load(file_path, &width, &height, NULL, 4);
            if (pixels == NULL) 
            {
                // TODO: don't crash the entire program on failed to open file.
                // Just continue processing the files.
                fprintf(stderr, "ERROR: could not read file %s\n", file_path);
                exit(1);
            }

            int resized_height = height*resized_width/width;
            resized_height = (float) resized_height * fixed;

            // TODO: maybe use a custom resize algorithm that does not require any memory allocation?
            // Similar to how olive.c resize the sprites.
            // (Though stb_image_resize supports a lot of fancy filters and stuff which we may try
            // to utilize to improve the results)
            uint32_t *resized_pixels = malloc(sizeof(uint32_t)*resized_width*resized_height);
            if (resized_pixels == NULL) 
            {
                fprintf(stderr, "ERROR: could not allocate memory for resized image\n");
                exit(1);
            }

            // TODO: stbir_resize_uint8 returns int, which means it can fail. We should check for that.
            stbir_resize_uint8
            (
                (const unsigned char*)pixels, width, 
                height, sizeof(uint32_t)*width,
                (unsigned char*)resized_pixels, 
                resized_width, resized_height, 
                sizeof(uint32_t)*resized_width,
                4
            );

            // allocate memory for ansi_index
            ansi_index = malloc( sizeof (uint8_t) * resized_height * resized_width );
            int thread_index = 0;
            int thread_id = 0;
            arguments *args = malloc( sizeof (arguments) * thread_count );
            pthread_t *tid = malloc( sizeof (pthread_t) * thread_count );
            memset( tid, 0, sizeof (pthread_t) * thread_count );

            for (int y = 0; y < resized_height; ++y) 
            {
                for (int x = 0; x < resized_width; ++x) 
                {
                    uint32_t pixel = resized_pixels[y*resized_width + x];
                    int r = (pixel>>8*0)&0xFF;
                    int g = (pixel>>8*1)&0xFF;
                    int b = (pixel>>8*2)&0xFF;
                    int a = (pixel>>8*3)&0xFF;
                    r = a*r/255;
                    g = a*g/255;
                    b = a*b/255;
                    switch (distance) 
                    {
                        case DIST_HSL: 
                        {
                            int h, s, l;
                            rgb_to_hsl(r, g, b, &h, &s, &l);
                            thread_id = thread_index % thread_count;
                            if ( tid[thread_id] )
                            {
                                pthread_join( tid[thread_id], NULL );
                            }
                            args[thread_id] = (arguments)
                            {
                                .mode = DIST_HSL,
                                .a = h,
                                .b = s,
                                .c = l,
                                .i = thread_index,
                            };
                            pthread_create( &tid[thread_id], NULL, fill_ansi_index, (void*) &args[thread_id] );
                            thread_index++;
                        } break;

                        case DIST_RGB: 
                        {
                            thread_id = thread_index % thread_count;
                            if ( tid[thread_id] )
                            {
                                pthread_join( tid[thread_id], NULL );
                            }
                            args[thread_id] = (arguments)
                            {
                                .mode = DIST_RGB,
                                .a = r,
                                .b = g,
                                .c = b,
                                .i = thread_index,
                            };
                            pthread_create( &tid[thread_id], NULL, fill_ansi_index, (void*) &args[thread_id] );
                            thread_index++;
                        } break;

                        default: assert(0 && "unreachable");
                    }
                }
            }

            print_result( resized_height, resized_width );

            free(tid);
            free(args);
            free(ansi_index);
            free(resized_pixels);
            stbi_image_free(pixels);
        }
    }

    puts( "press \"return\" to exit..." );
    getchar();
    return 0;
}
// TODO: automated testing
// Since the whole behaviour of the program is represented by its stdout, it should be pretty easy to have some autotests