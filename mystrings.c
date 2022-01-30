#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#define DEFAULT_MINLENGTH 4   /* minlength por defecto */
#define MAX_MINLENGTH 255     /* minlength maximo */
#define DEFAULT_BUF_SIZE 1024 /* Tamaño por defecto del buffer */
#define MAX_BUF_SIZE 1048576  /* Tamaño maximo del buffer */
#define MIN 1                 /* Tamaño minimo del buffer y de minlength */

void print_help(char *program_name)
{
    fprintf(stderr, "Uso: %s [-t BUFSIZE] [-n MINLENGTH]\n", program_name);
    fprintf(stderr, "las cadenas compuestas por caracteres imprimibles incluyendo espacios, tabuladores y saltos de línea, que tengan una longitud mayor o igual a un tamaño dado.\n");
    fprintf(stderr, "-t BUFSIZE     Tamaño de buffer donde MINLENGTH <= BUFSIZE <= 1MB (por defecto 1024).\n");
    fprintf(stderr, "-n MINLENGTH   Longitud mínima de la cadena. Mayor que 0 y menor que 256 (por defecto 4).\n");
}

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

void find_strings(char *buf_lectura, char *buf_escritura, int buf_size, int minlength)
{
    int contador = 0; // indica la longitud de una cadena de caracteres imprimibles
    ssize_t num_read, num_written;
    int total_read;
    char *buf_aux;
    if ((buf_aux = (char *)malloc(minlength * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    int k = 0;    // indice con el que se recorre el buffer auxiliar
    int j = 0;    // indice con el que se recorre el buffer de escritura
    int elem = 0; // indica cuantos elementos tiene el buffer de escritura
    while ((num_read = read_all(STDIN_FILENO, buf_lectura, buf_size)) > 0)
    {
        int i = 0; // indice con el que se recorre el buffer de lectura
        while (i < num_read)
        {
            if (isprint(buf_lectura[i]) || (buf_lectura[i] == '\n') || (buf_lectura[i] == '\t'))
            {
                contador++;
                buf_aux[k % minlength] = buf_lectura[i];
                k++;
            }
            else
            {
                contador = 0;
                k = 0;
            }
            // si hemos contado exactamente minlength caracteres imprimibles seguidos entonces escribimos todos esos caracteres en el buffer de escritura
            if (contador == minlength)
            {
                for (int l = 0; l < minlength; l++)
                {
                    buf_escritura[j] = buf_aux[l];
                    j++;
                    elem++;
                    /* Escribimos el contenido del buffer de escritura cuando esté lleno al completo y ponemos a 0 el índice con el se recorre y la variable que indica cuántos elementos tiene el buffer (es como si "reiniciásemos" el buffer para volverlo a llenar) */
                    if (j >= buf_size)
                    {
                        num_written = write_all(STDOUT_FILENO, buf_escritura, buf_size);
                        if (num_written == -1)
                        {
                            perror("write(fdin)");
                            exit(EXIT_FAILURE);
                        }
                        j = 0;
                        elem = 0;
                    }
                }
                k = 0;
            }
            // en el caso de que se cuenten más caracteres imprimibles seguidos que minlength "añadimos" el caracter a la secuencia
            else if (contador > minlength)
            {
                buf_escritura[j] = buf_aux[(k - 1) % minlength];
                j++;
                elem++;
            }
            /* Escribimos el contenido del buffer de escritura cuando esté lleno al completo y ponemos a 0 el índice con el se recorre y la variable que indica cuántos elementos tiene el buffer (es como si "reiniciásemos" el buffer para volverlo a llenar) */
            if (j >= buf_size)
            {
                num_written = write_all(STDOUT_FILENO, buf_escritura, buf_size);
                if (num_written == -1)
                {
                    perror("write(fdin)");
                    exit(EXIT_FAILURE);
                }
                j = 0;
                elem = 0;
            }
            i++;
        }
    }
    /* Una vez terminado de leer de la entrada comprobamos si nos quedan elementos en el buffer de escritura por escribir y los escribimos */
    if (elem > 0)
    {
        num_written = write_all(STDOUT_FILENO, buf_escritura, elem);
        if (num_written == -1)
        {
            perror("write(fdin)");
            exit(EXIT_FAILURE);
        }
    }

    if (num_read == -1)
    {
        perror("read(fdin)");
        exit(EXIT_FAILURE);
    }

    free(buf_aux);
}

int main(int argc, char *argv[])
{
    int opt, buf_size, minlenght;
    char *buf_lectura;
    char *buf_escritura;

    optind = 1;
    buf_size = DEFAULT_BUF_SIZE;
    minlenght = DEFAULT_MINLENGTH;

    while ((opt = getopt(argc, argv, "t:n:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            buf_size = atoi(optarg);
            break;
        case 'n':
            minlenght = atoi(optarg);
            break;
        case 'h':
        default:
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if ((buf_size < minlenght) || (buf_size > MAX_BUF_SIZE))
    {
        fprintf(stderr, "El tamaño del buffer debe ser mayor o igual que MINLENGTH y menor o igual que 1MB\n");
        print_help(argv[0]);
        return (EXIT_FAILURE);
    }

    if ((minlenght < MIN) || (minlenght > MAX_MINLENGTH))
    {
        fprintf(stderr, "La longitud mínima de cadena tiene que ser mayor que 0 y menor de 256.\n");
        print_help(argv[0]);
        return (EXIT_FAILURE);
    }

    /* Reserva memoria dinámica para los buffers de lectura y escritura */
    if ((buf_lectura = (char *)malloc(buf_size * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }
    if ((buf_escritura = (char *)malloc(buf_size * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    find_strings(buf_lectura, buf_escritura, buf_size, minlenght);

    /* Libera memoria dinámica de buffer de lectura y escritura*/
    free(buf_lectura);
    free(buf_escritura);

    exit(EXIT_SUCCESS);
}