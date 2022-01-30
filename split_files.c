#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define MAX_SIZE 1048576      /* Tamaño máximo del buffer: 1MB */
#define MIN_SIZE 1            /* Tamaño mínimo del buffer: 1B */
#define DEFAULT_BUF_SIZE 1024 /* Tamaño por defecto del buffer */
#define MAX_FILES 16          /* Número máximo de ficheros de salida */

ssize_t read_all(int fd, char *buf, size_t size)
{
    ssize_t num_read = 0;
    ssize_t num_left = size;
    char *buf_left = buf;

    while ((num_left > 0) && (num_read = read(fd, buf_left, num_left)) > 0)
    {
        buf_left += num_read;
        num_left -= num_read;
    }
    if (num_read == -1)
        return -1;
    else
        return (size - num_left);
}

ssize_t write_all(int fd, char *buf, size_t size)
{
    ssize_t num_written = 0;
    ssize_t num_left = size;
    char *buf_left = buf;

    while ((num_left > 0) && (num_written = write(fd, buf_left, num_left)) != -1)
    {
        buf_left += num_written;
        num_left -= num_written;
    }
    return num_written == -1 ? -1 : size; // si num_written vale -1 devuelvo num_writte, si no devuelvo size
}

void split(int fdout[], char *buf_escritura[], char *buf_lectura, int buf_size, int n_files)
{
    ssize_t num_read, num_written;
    int j = 0;           /* con este índice insertamos en los buffers de escritura */
    int buf = 0;         /* índice para saber en qué buffer insertar */
    int n_elem[n_files]; /* almacenamos el nº de elementos que hay en cada buffer de escritura */
    for (int q = 0; q < n_files; q++)
        n_elem[q] = 0;

    while ((num_read = read_all(STDIN_FILENO, buf_lectura, buf_size)) > 0)
    {
        for (int i = 0; i < num_read; i++)
        {
            buf_escritura[buf % n_files][j] = buf_lectura[i];
            n_elem[buf % n_files]++;
            buf++;
            if ((buf % n_files) == 0)
                j++;

            /* Si los buffers están llenos escribimos su contenido en los ficheros de salida y "reiniciamos" los índices y el nº de elem que hay en cada buffer de escritura */
            if (((j % buf_size) == 0) && (j != 0))
            {
                for (int k = 0; k < n_files; k++)
                {
                    num_written = write_all(fdout[k], buf_escritura[k], n_elem[k]);
                    if (num_written == -1)
                    {
                        perror("write(fdin)");
                        exit(EXIT_FAILURE);
                    }
                }
                j = 0;
                for (int l = 0; l < n_files; l++)
                    n_elem[l] = 0;
            }
        }
    }

    /* En el caso de que toda la entrada "quepa" directamente en los buffers de escritura o estemos al final de la entrada escribimos */
    if (num_read == 0)
    {
        for (int i = 0; i < n_files; i++)
        {
            num_written = write_all(fdout[i], buf_escritura[i], n_elem[i]);
            if (num_written == -1)
            {
                perror("write(fdin)");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (num_read == -1)
    {
        perror("read(fdin)");
        exit(EXIT_FAILURE);
    }
}

void print_help(char *program_name)
{
    fprintf(stderr, "Uso: %s [-t BUFSIZE] FILEOUT1 [FILEOUT2 ... FILEOUTn] \n", program_name);
    fprintf(stderr, "Divide en ficheros el flujo de bytes que recibe por la entrada estandar\n");
    fprintf(stderr, "El numero maximo de de ficheros de salida es 16.\n");
    fprintf(stderr, "-t BUFSIZE  Tamaño de buffer donde 1 <= BUFSIZE <= 1MB (por defecto 1024).\n");
}

int main(int argc, char *argv[])
{
    int opt, buf_size, n_files;
    char *buf_lectura;

    optind = 1;
    buf_size = DEFAULT_BUF_SIZE;

    while ((opt = getopt(argc, argv, "t:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            buf_size = atoi(optarg);
            break;
        case 'h':
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if ((buf_size < MIN_SIZE) || (buf_size > MAX_SIZE))
    {
        fprintf(stderr, "Error: Tamaño de buffer incorrecto\n");
        print_help(argv[0]);
        return (EXIT_FAILURE);
    }
    if (argc == optind)
    {
        fprintf(stderr, "Error: No hay ficheros de salida.\n");
        print_help(argv[0]);
        return (EXIT_FAILURE);
    }
    if (argc - optind > MAX_FILES)
    {
        fprintf(stderr, "Error: Demasiados ficheros de salida.\n");
        print_help(argv[0]);
        return (EXIT_FAILURE);
    }

    n_files = argc - optind;

    /* Reserva memoria dinámica para los buffers de lectura y escritura */
    if ((buf_lectura = (char *)malloc(buf_size * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }
    char *buf_escritura[n_files];
    for (int i = 0; i < n_files; i++)
    {
        if ((buf_escritura[i] = (char *)malloc(buf_size * sizeof(char))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
    }

    /* Abre los ficheros de salida */
    int a_fdout[n_files];
    for (int i = optind; i < argc; i++)
    {
        a_fdout[i - optind] = open(argv[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (a_fdout[i - optind] == -1)
        {
            perror("Error: No se puede abrir/crear el fichero");
            exit(EXIT_FAILURE);
        }
    }

    split(a_fdout, buf_escritura, buf_lectura, buf_size, n_files);

    /* Cierra los ficheros de salida */
    for (int i = 0; i < n_files; i++)
    {
        if (a_fdout[i] != -1)
        {
            if (close(a_fdout[i]) == -1)
            {
                perror("close(fdout)");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* Libera memoria dinámica de buffer de lectura y escritura*/
    free(buf_lectura);
    for (int i = 0; i < n_files; i++)
    {
        free(buf_escritura[i]);
    }

    exit(EXIT_SUCCESS);
}