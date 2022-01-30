#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#define MAX_SIZE 134217728    /* Tamaño máximo del buffer: 128MB */
#define MIN_SIZE 1            /* Tamaño mínimo del buffer: 1B */
#define DEFAULT_BUF_SIZE 1024 /* Tamaño por defecto del buffer */
#define MAX_FILES 16          /* Número máximo de ficheros admitidos */

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

void merge_files(int fdout, int *a_fdin, char *buf_lectura[], char *buf_escritura, int buf_size, int n_files)
{
    int todos_leidos = 0;
    ssize_t *num_read;
    int *leido;

    if ((num_read = malloc(n_files * sizeof(ssize_t))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    if ((leido = malloc(n_files * sizeof(int))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n_files; i++)
        leido[i] = 0;

    // todos_leidos y leido[] se tratan como un un valor booleano y un array de booleanos, respectivamente
    // todos_leidos = 0 significa que no han sido leidos al completo todos los ficheros
    // leido[i] = 0 siginifica que el fichero i no ha sido leido al completo
    while (todos_leidos == 0)
    {
        ssize_t bytes_read = 0;
        for (int i = 0; i < n_files; i++)
        {
            if (leido[i] == 0)
            {
                if (a_fdin[i] != -1) // Para leer tenemos que asegurarnos que es un fichero válido
                    num_read[i] = read_all(a_fdin[i], buf_lectura[i], buf_size);
                else
                    num_read[i] = 0;
                if (num_read[i] == -1)
                {
                    perror("read(fdin)");
                    exit(EXIT_FAILURE);
                }
                if (num_read[i] == 0)
                    leido[i] = 1;
                bytes_read = bytes_read + num_read[i];
            }
        }

        /* Mezclamos en buf_escritura los bytes de cada fichero */
        int file = 0;
        int offset = 0;
        int contador = 0; // Cuenta cuántos bytes se han escrito de los buffers de lectura en el buffer de escritura
        int aux = 0;      // Variable auxiliar que sirve para calcular el offset dentro de un buf_lectura
        while (contador < bytes_read)
        {
            int i = 0;
            while ((i < buf_size) && (contador < bytes_read))
            {
                if (offset < num_read[file])
                {
                    contador++;
                    buf_escritura[i] = buf_lectura[file][offset];
                    i++;
                }
                aux = aux + 1;
                offset = aux / n_files;
                file = (file + 1) % n_files;
            }

            /* Una vez se haya llenado todo el buffer de escritura (puede no ser así al final), se escribe su contenido en la salida */
            ssize_t num_written = write_all(fdout, buf_escritura, i);
            if (num_written == -1)
            {
                perror("write(fdin)");
                exit(EXIT_FAILURE);
            }
        }

        /* Comprobamos si se han leido al completo todos los ficheros */
        int l = 0;
        while ((leido[l] == 1) && (l < n_files))
            l++;
        if (l >= n_files)
            todos_leidos = 1;
        else
            todos_leidos = 0;
    }

    /* Libera memoria dinámica de los arrays leido y num_read */
    free(num_read);

    free(leido);
}

void print_help (char* program_name)
{
    fprintf(stderr, "Uso: %s [-t BUFSIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\n", program_name);
    fprintf(stderr, "No admite lectura de la entrada estandar.\n");
    fprintf(stderr, "-t BUFSIZE  Tamaño de buffer donde 1 <= BUFSIZE <= 128MB\n");
    fprintf(stderr, "-o FILEOUT  Usa FILEOUT en lugar de la salida estandar\n");
}

int main(int argc, char *argv[])
{
    int opt, buf_size, fdout;
    char *fileout = NULL;
    char *buf_escritura;
    int *a_fdin;

    optind = 1;
    buf_size = DEFAULT_BUF_SIZE;
    while ((opt = getopt(argc, argv, "t:o:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            buf_size = atoi(optarg);
            break;
        case 'o':
            fileout = optarg;
            break;
        case 'h':
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (argc == optind)
    {
        fprintf(stderr, "Error: No hay ficheros de entrada.\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (argc - optind > MAX_FILES)
    {
        fprintf(stderr, "Error: Demasiados ficheros de entrada. Máximo 16 ficheros.\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (buf_size < MIN_SIZE || buf_size > MAX_SIZE)
    {
        fprintf(stderr, "Error: Tamaño de buffer incorrecto.\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Abre el fichero de salida */
    if (fileout != NULL)
    {
        fdout = open(fileout, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fdout == -1)
        {
            perror("open(fileout)");
            exit(EXIT_FAILURE);
        }
    }
    else /* Por defecto, la salida estándar */
    {
        fdout = STDOUT_FILENO;
    }

    int n_files = argc - optind;

    /* Reserva memoria dinámica para el array que almacena los descriptores de los ficheros de entrada */
    if ((a_fdin = malloc(n_files * sizeof(int))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    /* Reserva memoria dinámica para los buffers de lectura y escritura */
    char *buf_lectura[n_files];
    for (int i = 0; i < n_files; i++)
    {
        if ((buf_lectura[i] = (char *)malloc(buf_size * sizeof(char))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
    }
    if ((buf_escritura = (char *)malloc(buf_size * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    for (int i = optind; i < argc; i++)
    {
        int fdin = open(argv[i], O_RDONLY);
        a_fdin[i - optind] = fdin;
        if (fdin == -1)
        {
            perror("Error: No se ha podido abrir el fichero");
            continue;
        }
    }
    merge_files(fdout, a_fdin, buf_lectura, buf_escritura, buf_size, n_files);
    for (int i = 0; i < n_files; i++)
    {
        if (a_fdin[i] != -1)
        {
            if (close(a_fdin[i]) == -1)
            {
                perror("close(fdin)");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (close(fdout) == -1)
    {
        perror("close(fdout)");
        exit(EXIT_FAILURE);
    }

    /* Libera memoria dinámica de buffers de lectura, escritura y array de descriptores de ficheros */
    for (int i = 0; i < n_files; i++)
        free(buf_lectura[i]);
        
    free(buf_escritura);

    free(a_fdin);

    exit(EXIT_SUCCESS);
}