#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#define DEFAULT_MINLENGTH 4   /* minlength por defecto */
#define MAX_MINLENGTH 255     /* minlength maximo */
#define DEFAULT_BUF_SIZE 1024 /* Tamaño por defecto del buffer */
#define MAX_BUF_SIZE 1048576  /* Tamaño maximo del buffer */
#define MIN 1                 /* Tamaño minimo del buffer y de minlength */

void print_help(char *program_name)
{
    fprintf(stderr, "Uso: %s [-t BUFSIZE] [-n MIN_LENGTH] -i FILEIN1[,FILEIN2,...,FILEINn] FILEOUT1 [FILEOUT2 ...]\n", program_name);
    fprintf(stderr, "-t BUFSIZE      Tamaño de buffer donde 1 <= BUFSIZE <= 1MB (por defecto 1024).\n");
    fprintf(stderr, "-n MINLENGTH    Longitud mínima de la cadena. Mayor que 0 y menor que 256 (por defecto 4).\n");
    fprintf(stderr, "-i              Lista de ficheros de entrada separados por comas.\n");
}

/* Extrae los ficheros de salida y devuelve un array con los argumentos que hay que pasarle a merge */
char **merge_args(char *input_files, char *buf_size, int t)
{
    char *saveptr;
    int i = 0;
    int n_files = 1; /* Lo usamos para contar el numero de ficheros de entrada de la cadena que será: nº de comas +1*/

    while (input_files[i] != '\0')
    {
        if (input_files[i] == ',')
        {
            n_files++;
        }
        i++;
    }

    char **arg = NULL;
    if (t)
    {
        if ((arg = (char **)malloc((4 + n_files) * sizeof(char *))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
        arg[0] = "./merge_files";
        arg[1] = "-t";
        arg[2] = buf_size;
        int j = 0;
        arg[j + 3] = strtok_r(input_files, ",", &saveptr);
        while (arg[j + 3] != NULL)
        {
            j++;
            arg[j + 3] = strtok_r(NULL, ",", &saveptr);
        }
        arg[3 + n_files] = (char *)NULL;
    }
    else
    {
        if ((arg = (char **)malloc((2 + n_files) * sizeof(char *))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
        arg[0] = "./merge_files";
        int j = 0;
        arg[1] = strtok_r(input_files, ",", &saveptr);
        while (arg[j + 1] != NULL)
        {
            j++;
            arg[j + 1] = strtok_r(NULL, ",", &saveptr);
        }
        arg[1 + n_files] = (char *)NULL;
    }

    return arg;
}

int main(int argc, char *argv[])
{
    int opt, pid1, pid2, pid3;
    int t = 0; /* Indica si se ha pasado -t por parametro */
    int n = 0; /* Indica si se ha pasado -n por parametro */
    int i = 0; /* Indica si se ha pasado -i por parametro */
    char *input_files = NULL;
    char *buf_size = NULL;
    char *minlength = NULL;

    optind = 1;

    while ((opt = getopt(argc, argv, "t:n:i:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            buf_size = optarg;
            t = 1;
            break;
        case 'n':
            minlength = optarg;
            n = 1;
            break;
        case 'i':
            input_files = optarg;
            i = 1;
            break;
        case 'h':
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if ((t) && ((atoi(buf_size) > MAX_BUF_SIZE) || (atoi(buf_size) < MIN)))
    {
        fprintf(stderr, "Error: El tamaño de buffer debe ser mayor que 0 y menor que 1048576\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }
    if ((n) && ((atoi(minlength) > MAX_MINLENGTH) || (atoi(minlength) < MIN)))
    {
        fprintf(stderr, "Error: La longitud mínima de cadena debe ser mayor que 0 y menor de 256\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (!i)
    {
        fprintf(stderr, "Error: Deben proporcionarse ficheros de entrada con la opción -i\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc == optind)
    {
        fprintf(stderr, "Error: Debe proporcionarse la lista de ficheros de salida\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    int pipefds1[2]; /* Descriptores de fichero de la tubería 1 */
    int pipefds2[2]; /* Descriptores de fichero de la tubería 2 */

    if (pipe(pipefds1) == -1) /* Paso 0: Creación de las tuberías */
    {
        perror("pipe1()");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipefds2) == -1) /* Paso 0: Creación de las tuberías */
    {
        perror("pipe2()");
        exit(EXIT_FAILURE);
    }

    /* Paso 1: Creación del proceso hijo merge_files*/
    /* Creación del array que contiene los argumentos que hay que pasar a merge_files */
    char **arg_merge = merge_args(input_files, buf_size, t);

    switch (pid1 = fork())
    {
    case -1:
        perror("fork(1)");
        exit(EXIT_FAILURE);
        break;
    case 0: /* Tubería 1 */
        /* Paso 2: El extremo de lectura de la tubería 1 no se usa */
        if (close(pipefds1[0]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }
        /* Paso 3: Redirige la salida estándar al extremo de escritura de la tubería 1*/
        if (dup2(pipefds1[1], STDOUT_FILENO) == -1)
        {
            perror("dup2(1)");
            exit(EXIT_FAILURE);
        }
        /* Paso 4: Cierra resto de extremos */
        if (close(pipefds1[1]) == -1)
        {
            perror("close(pipefds1[1])");
            exit(EXIT_FAILURE);
        }
        if (close(pipefds2[0]) == -1)
        {
            perror("close(pipefds2[0]");
            exit(EXIT_FAILURE);
        }
        if (close(pipefds2[1]) == -1)
        {
            perror("close(pipefds2[1]");
            exit(EXIT_FAILURE);
        }
        /* Paso 5: Reemplaza el binario actual por el de merge_files */
        execv("./merge_files", arg_merge);
        perror("execv(merge_files)");
        exit(EXIT_FAILURE);
        break;
    default: /* El proceso padre continúa... */
        break;
    }

    /* Paso 6: Creación del proceso hijo mystrings */
    switch (pid2 = fork())
    {
    case -1:
        perror("fork(2)");
        exit(EXIT_FAILURE);
        break;
    case 0: /* Tuberías 1 y 2 */
        /* Paso 7: El extremo de escritura de la tubería 1 no se usa */
        if (close(pipefds1[1]) == -1)
        {
            perror("close(pipefds1[1])");
            exit(EXIT_FAILURE);
        }
        /* Paso 8: El extremo de lectura de la tubería 2 no se usa */
        if (close(pipefds2[0]) == -1)
        {
            perror("close(pipefds2[2])");
            exit(EXIT_FAILURE);
        }
        /* Paso 8: Redirige la entrada estándar al extremo de lectura de la tubería 1*/
        if (dup2(pipefds1[0], STDIN_FILENO) == -1)
        {
            perror("dup2(2)");
            exit(EXIT_FAILURE);
        }
        /* Paso 9: Redirige la salida estándar al extremo de escritura de la tubería 2*/
        if (dup2(pipefds2[1], STDOUT_FILENO) == -1)
        {
            perror("dup2(2)");
            exit(EXIT_FAILURE);
        }
        /* Paso 10: Cierra resto de extremos */
        if (close(pipefds1[0]) == -1)
        {
            perror("close(4)");
            exit(EXIT_FAILURE);
        }
        if (close(pipefds2[1]) == -1)
        {
            perror("close(4)");
            exit(EXIT_FAILURE);
        }
        /* Paso 11: Reemplaza el binario actual por el de mystrings */
        if (t && n)
        {
            execlp("./mystrings_v2", "./mystrings_v2", "-t", buf_size, "-n", minlength, NULL);
            perror("execlp(mystrings)");
            exit(EXIT_FAILURE);
            break;
        }
        else if (t && !n)
        {
            execlp("./mystrings", "./mystrings", "-t", buf_size, NULL);
            perror("execlp(mystrings)");
            exit(EXIT_FAILURE);
            break;
        }
        else if (!t && n)
        {
            execlp("./mystrings", "./mystrings", "-n", minlength, NULL);
            perror("execlp(mystrings)");
            exit(EXIT_FAILURE);
            break;
        }
        else if (!t && !n)
        {
            execlp("./mystrings", "./mystrings", NULL);
            perror("execlp(mystrings)");
            exit(EXIT_FAILURE);
            break;
        }

    default: /* El proceso padre continúa... */
        break;
    }

    /* Creación del proceso de split_files */
    /* Creación del array que contiene los argumentos que hay que pasarle a split_files */
    int n_output_files = argc - optind;
    char **arg_split = NULL;
    if (t)
    {
        if ((arg_split = (char **)malloc((4 + n_output_files) * sizeof(char *))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
        arg_split[0] = "./split_files";
        arg_split[1] = "-t";
        arg_split[2] = buf_size;
        for (int k = 0; k < n_output_files; k++)
            arg_split[3 + k] = argv[optind + k];
        arg_split[n_output_files + 3] = (char *)NULL;
    }
    else
    {
        if ((arg_split = (char **)malloc((2 + n_output_files) * sizeof(char))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
        arg_split[0] = "./split_files";
        for (int k = 0; k < n_output_files; k++)
            arg_split[1 + k] = argv[optind + k];
        arg_split[n_output_files + 1] = (char *)NULL;
    }

    switch (pid3 = fork())
    {
    case -1:
        perror("fork(2)");
        exit(EXIT_FAILURE);
        break;
    case 0: /* Tubería 2 */
        /* Paso 12: El extremo de escritura de la tubería 2 no se usa */
        if (close(pipefds2[1]) == -1)
        {
            perror("close()");
            exit(EXIT_FAILURE);
        }
        /* Paso 13: Redirige la entrada estándar al extremo de lectura de la tubería 2*/
        if (dup2(pipefds2[0], STDIN_FILENO) == -1)
        {
            perror("dup2()");
            exit(EXIT_FAILURE);
        }
        /* Paso 14: Cierra resto de extremos */
        if (close(pipefds2[0]) == -1)
        {
            perror("close()");
            exit(EXIT_FAILURE);
        }
        if (close(pipefds1[0]) == -1)
        {
            perror("close()");
            exit(EXIT_FAILURE);
        }
        if (close(pipefds1[1]) == -1)
        {
            perror("close()");
            exit(EXIT_FAILURE);
        }
        /* Paso 15: Reemplaza el binario actual por el de split_files */
        execv("./split_files", arg_split);
        perror("execlp(split_files)");
        exit(EXIT_FAILURE);
        break;
    default: /* El proceso padre continúa... */
        break;
    }

    /* El proceso padre cierra los descriptores de fichero no usados */
    if (close(pipefds1[0]) == -1)
    {
        perror("close(pipefds1[0])");
        exit(EXIT_FAILURE);
    }
    if (close(pipefds1[1]) == -1)
    {
        perror("close(pipefds1[1])");
        exit(EXIT_FAILURE);
    }
    if (close(pipefds2[0]) == -1)
    {
        perror("close(pipefds2[0])");
        exit(EXIT_FAILURE);
    }
    if (close(pipefds2[1]) == -1)
    {
        perror("close(pipefds2[1])");
        exit(EXIT_FAILURE);
    }

    /* El proceso padre espera a que terminen sus procesos hijo */
    for (int i = 0; i < 3; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait()");
            exit(EXIT_FAILURE);
        }
    }

    free(arg_split);
    free(arg_merge);

    exit(EXIT_SUCCESS);
}