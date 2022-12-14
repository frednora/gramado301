
// kstdio.c

#include <kernel.h>

//#todo: Use default lib.
#define HEX_LEN  8
#define ____INT_MAX  2147483647

//
// File pointers.
//

// Standard stream.
// Initialized by kstdio_initialize() in kstdio.c
file *stdin;
file *stdout;
file *stderr;

// VFS
// Not initialized yet.
file *vfs_fp;

// Rootdir for bootvolume and systemvolume.
// Initialized by fsInit() in fs.c
file *volume1_rootdir_fp;
file *volume2_rootdir_fp;

//...

// 0
//Pipe para a rotina execve particular 
//usada no no init do ambiente Gramado Core.
file *pipe_gramadocore_init_execve;

// 1
//Pipe usado pela rotina execve.
file *pipe_execve;




static void __clear_prompt_buffers(void);

static void __initialize_file_table(void);
static void __initialize_inode_table(void);

static void __initialize_stdin(void);
static void __initialize_stdout(void);
static void __initialize_stderr(void);

static void __initialize_virtual_consoles(void);

static char *_vsputs_r(char *dest, char *src);

// service 8002
// IN: fd for the new stdin
// OUT: TRUE or FALSE.

int sys_setup_stdin(int stdin_fd)
{
    struct process_d *p;
    file *f;

    pid_t current_process = (pid_t) get_current_process();

    if( current_process < 0 || current_process >= PROCESS_COUNT_MAX )
        return FALSE;

    p = (struct process_d *) processList[current_process];

    if ( (void*)p==NULL )
        return FALSE;

    if(p->used != TRUE)
        return FALSE;

    if(p->magic != 1234)
        return FALSE;


    if(stdin_fd < 0 || stdin_fd >= 32)
        return FALSE;

    f = (file *) p->Objects[stdin_fd];

    if ( (void*)f==NULL )
        return FALSE;

    if(f->used != TRUE)
        return FALSE;

    if(f->magic != 1234)
        return FALSE;


// The new stdin.
    stdin = (file *) f;
// Permission for writing.
    stdin->sync.can_write = TRUE;
// flags
    stdin->_flags |= __SWR;

// ok, we have a new stdin.
// this way the keyboard can put the bytes into this new file.
    return TRUE;
}

int is_socket(file *f)
{
// Fail
    if ( (void *) f == NULL ){ return FALSE; }
// Yes
    if ( f->____object == ObjectTypeSocket ){ return TRUE; }
// No
    return FALSE;
}

int is_virtual_console(file *f)
{
// Fail
    if ( (void *) f == NULL ){ return FALSE; }
// Yes
    if ( f->____object == ObjectTypeVirtualConsole ){ return TRUE; } 
// No
    return FALSE;
}

// Kernel size version of the
// standard input() libc function.
unsigned long kinput ( unsigned long ch )
{
    int i=0;
    char c = (char) ch;

// Input mode.
// #bugbug:
// Estamos nos referindo ao tipo de linha, 
// se ?? simples ou multiplas linhas.
// Mas temos outra flag relativa ?? imput mode 
// que trata dos eventos.
// See: ???
// Onde est??o as flags ???

    if ( g_inputmode == INPUT_MODE_LINE )
    {
        if (prompt_pos >= PROMPT_SIZE)
        {
            //printf ("kstdio-input: [FAIL] INPUT_MODE_LINE full buffer!\n");
            goto fail;
            // refresh_screen ();
            //return (unsigned long) 0; 
        }
    }

    //tem que ter o tamanho de um arquivo.
    if (g_inputmode == INPUT_MODE_MULTIPLE_LINES )
    {
        if (prompt_pos >= PROMPT_SIZE)
        {
            //printf("kstdio-input: [FAIL] INPUT_MODE_MULTIPLE_LINES full buffer\n");
            goto fail;
            //refresh_screen();
            //return (unsigned long) 0; 
        }
    }

// Trata o caractere digitado. 

    switch (c){

    // Enter.
    // +se for modo comando devemos finalizar com zero.
    // +se for modo texto, devemos apenas incluir os caracteres \r \n.
    //case 0x1C:
    case VK_RETURN:
        //modo linha 
        if (g_inputmode == INPUT_MODE_LINE )
        {
            prompt[prompt_pos] = (char )'\0'; //end of line.
            //#todo: ?? ldiscCompare();
            // o compare est?? no aplicativo.
            for(i=0; i<PROMPT_MAX_DEFAULT; i++)
            {
                prompt[i]     = (char) '\0';
                prompt_out[i] = (char) '\0';
                prompt_err[i] = (char) '\0';
            };
            prompt_pos=0;
            goto input_done;
        }
        //modo multiplas linhas 
        if (g_inputmode == INPUT_MODE_MULTIPLE_LINES )
        {
            prompt[prompt_pos] = (char )'\r';  prompt_pos++;
            prompt[prompt_pos] = (char )'\n';  prompt_pos++;
        }
        break;

    // Backspace
    case 0x0E:
        if ( prompt_pos <= 0 )
        {
            prompt_pos = 0;
            prompt[prompt_pos] = (char ) '\0';
            break; 
        }
        // Apaga o anterior (no buffer).
        prompt_pos--;
        prompt[prompt_pos] = (char ) '\0';
        break;

    // ...

    // Para qualquer caractere que n??o sejam 
    // os especiais tratados acima.
    default:
        prompt[prompt_pos] = c;
        prompt_pos++;
        break;
    };

input_more:
    return 0;
input_done:
    return VK_RETURN;
fail:
    refresh_screen();
    return (unsigned long) 0; 
}


/*
 * printchar:
 *     Coloca o caractere na string ou imprime.
 * Essa fun????o chama uma rotina que dever?? tratar o caractere e 
 * em seguida envi??-lo para a tela.
 * Essa rotina ?? chamada pelas fun????es: /print/printi/prints.
 */
// #importante
// Se a string existe colocamos nela,
// caso contr??rio imprimimos no backbuffer.
// Vamos aproveitar esse momento para ativarmos a
// pintura no caso dos caraters enviados para uma 
// stream de output, como stdout.
// Ativaremos a rotina de mostrar na tela s?? no momento em que 
// encontramos um fim de linha.
// IN:
//     **str // #dangerdanger
//     c

void printchar (char **str, int c)
{
    if (str){

        if ( c == '\n' ){
            // tty->print_pending = 1;
        }
        
        // #dangerdanger
        **str = c;

        ++(*str);

    } else (void) putchar(c);
}


void putchar_K(void)
{
    //while(1){
        putchar('K');
        //putchar('\n');
        //refresh_screen();
    //}
}


/*
 * putchar:
 *     Put a char on the screen. (libC).
 *     Essa rotina chama uma rotina de tratamento de caractes, 
 * somente depois ?? que o caractere ser?? enviado para a tela.
 *     Essa rotina ?? chamada pelas fun????es: /printchar/input/.
 */

// Para virtual consoles.
// Em tty/console.c

int putchar (int ch)
{

// #todo
// Maybe we can filter the printable chars
// for the console font.

    if(ch<0){
        return (int)(-1);
    }

    if(fg_console<0){
        return (int)(-1);
    }

    console_outbyte ( ch, fg_console );

    return (int) ch;
}


/*
 *  ==== Segue aqui o suporte a fun????o 'printf' ====
 *
 * #obs:
 * Em user mode temos uma modelo mais tradicional de printf,
 * talvez seja bom implementa-lo aqui tamb??m.
 */

/*
 * prints:
 *     Rotina de suporta a printf. 
 */
 
// #bugbug
// We need to create a cleaner routine.
// This one is a mess.
// '**out' #dangerdanger

int 
prints ( 
    char **out, 
    const char *string, 
    int width, 
    int pad )
{
    register int pc = 0, padchar = ' ';
    register int len = 0;
    register const char *ptr;

    if (width > 0) 
    {
        // ?? ugly
        for (ptr = string; *ptr; ++ptr) ++len;
        if (len >= width) width = 0;
        else width -= len;
        if (pad & PAD_ZERO) padchar = '0';
    }


    if ( !(pad & PAD_RIGHT) ) 
    {
        //#ugly
        for ( ; width > 0; --width)
        {
            printchar (out,padchar);
            ++pc;
        };
    }

    //#ugly
    for ( ; *string; ++string )
    {
        printchar (out, *string);
        ++pc;
    };

    //#ugly
    for ( ; width > 0; --width )
    {
        printchar (out,padchar);
        ++pc;
    };


    return (int) pc;
}


/*
 * printi:
 *     Rotina de suporta a printf.
 */

// '**out' #dangerdanger

int 
printi (
    char **out, 
    int i, 
    int b, 
    int sg, 
    int width, 
    int pad, 
    int letbase )
{

    char print_buf[PRINT_BUF_LEN];
    
    register char *s;
    register int t, neg = 0, pc = 0;
    
    // loop
    register unsigned int u = i;


    if ( i == 0 ){
        print_buf[0] = '0';
        print_buf[1] = '\0';
        return (int) prints (out, print_buf, width, pad);
    }

    if ( sg && 
         b == 10 && 
         i < 0 )
    {
        neg = 1;
        u = -i;
    }

    s = ( print_buf + ( PRINT_BUF_LEN -1 ) );

    *s = '\0';


    while (u)
    {
        t = (u % b);
        
        // ugly
        
        if ( t >= 10 )
            t += letbase - '0' - 10;
        
        *--s = t + '0';
        u /= b;
    };


    if (neg){
        if ( width && (pad & PAD_ZERO) ){
            printchar (out, '-');
            ++pc;
            --width;
        }else { 
            *--s = '-'; 
        };
    }

// #obs: 
// retorna pc + o retorno da fun????o.
// ugly shit
    
    return (int) pc + prints(out, s, width, pad);
}


/*
 * print:
 *     Rotina de suporta a printf.
 */
// #bugbug
// E se essa rotina for chamada com o primeiro argumento nulo?
// vai escrever na IVT ?
// Vai chamar alguma rotina passando esse emsmo endere??o de buffer.
//Aten??ao:
// print() nao analisa flags como runlevel ou kernel phase.

int print ( char **out, int *varg )
{
    register int width = 0;
    register int pad = 0;
    register int pc = 0;
    register char *format = (char *) (*varg++);
    char scr[2];

    // ugly
    for ( ; *format != 0; ++format ) 
    {
        if ( *format == '%' ) 
        {
            ++format;

            width = pad = 0;

            if ( *format == '\0' ) { break; }
            if ( *format == '%' )  { goto ____out; }

            if ( *format == '-' )
            {
                ++format;
                pad = PAD_RIGHT;
            }

            while ( *format == '0' ) 
            {
                ++format;
                pad |= PAD_ZERO;
            };

            // #ugly
            for ( ; 
                *format >= '0' && *format <= '9'; 
                ++format )
            {
                width *= 10;
                width += *format - '0';
            };

            //#ugly
            if ( *format == 's' ) 
            {
                // #bugbug: definition here.
                register char *s = *((char **)varg++);
                 pc += prints (out, s?s:"(null)", width, pad);

                continue;
            }

            if ( *format == 'd' ){
                pc += printi (out, *varg++, 10, 1, width, pad, 'a');
                continue;
            }

            if ( *format == 'x' ){
                pc += printi (out, *varg++, 16, 0, width, pad, 'a');
                continue;
            }

            if ( *format == 'X' ){
                pc += printi (out, *varg++, 16, 0, width, pad, 'A');
                continue;
            }

            if ( *format == 'u' ){
                pc += printi (out, *varg++, 10, 0, width, pad, 'a');
                continue;
            }

            // #ugly
            if ( *format == 'c' ) 
            {
                // 'char' are converted to int 
                // then pushed on the stack.
                scr[0] = *varg++;
                scr[1] = '\0';
                pc += prints (out, scr, width, pad);
                continue;
            }

        // Os caracteres normais diferentes de "%".
        }else{
            ____out:
                printchar (out, *format);
                ++pc;
        };
    };

// Se o buffer existe, finaliza a string?
    if (out){ 
        **out = '\0'; 
    }

    return (int) pc;
}


/*
 * printk:
 *     @field 2
 *     The printf function.
 *     Assuming sizeof(void *) == sizeof(int).
 * Em user mode temos uma modelo mais tradicional de printf,
 * talvez seja bom implementa-lo aqui tambem.
 */

// #bugbug
// #todo:
// Devemos tentar usar o mesmo printf implementado na libc
// Essa aqui n??o est?? no padr??o.
// #todo:
// Vamos substtuir essa fun????o por uma de licensa bsd.
// Olhar na biblioteca.
// #suspensa
// Essa implementa????o foi feita para 32bit e n??o funciona
// por inteiro em long mode.
// Usaremos kinguio_printf por enquanto.

int printk_old ( const char *format, ... )
{
    register int *varg = (int *) (&format);

    // #bugbug:
    
    // Se print() est?? usando '0' como buffer,
    // ent??o ele est?? sujando a IVT. 

    //Aten??ao:
    // print() nao analisa flags.

    return (int) print(0,varg);
}    


// print() is a helper function for this one.
/*
int vsprintf(char *string, const char *format, va_list ap);
int vsprintf(char *string, const char *format, va_list ap)
{
}
*/

// ===================================

void 
kinguio_i2hex( 
    unsigned int val, 
    char *dest, 
    int len )
{
    char *cp;
    int i=0; 
    int x=0;
    unsigned n;  //??


    if (val == 0)
    {
        cp = &dest[0];
        *cp++ = '0';
        *cp = '\0';
        return;
    }

    n = val;
    cp = &dest[len];

    while (cp > dest)
    {
        x = (n & 0xF);
        n >>= 4;
        
        // #
        *--cp = x + ((x > (HEX_LEN+1)) ? 'A' - 10 : '0');
    };

    dest[len] = '\0';

    cp = &dest[0];

    for (i=0; i < len; i++)
    {
        if (*cp == '0') {
            cp++;
        }else {
            strcpy (dest,cp);
            break;
        }
    }

    cp = &dest[0];
    n = strlen(cp);

    memset( (dest+n), 0, (8-n) );
}


char *kinguio_itoa (int val, char *str) 
{
    int value = val;
    char *valuestring = (char *) str;
    int min_flag=0;
    char swap=0; 
    char *p;

    if (0 > value)
    {
        *valuestring++ = '-';
        value = -____INT_MAX> value ? min_flag = ____INT_MAX : -value;
    }

    p = valuestring;

    do {
         *p++ = (char) (value % 10) + '0';
         value /= 10;
    } while (value);

    if (min_flag != 0)
    {
        ++*valuestring;
    }

    *p-- = '\0';

    while (p > valuestring)
    {
        swap = *valuestring;
        *valuestring++ = *p;
        *p-- = swap;
    };

    return str;
}


static char *_vsputs_r(char *dest, char *src)
{
    unsigned char *usrc = (unsigned char *) src;
    unsigned char *udest = (unsigned char *) dest;

    while ( *usrc )
    { 
        *udest++ = *usrc++; 
    };

    return (char *) udest;
}


int 
kinguio_vsprintf(
    char *str, 
    const char *fmt, 
    va_list ap )
{
    char *str_tmp = str;
    int index=0;
    unsigned char u=0;
    int d=0;
    char c=0; 
    char *s;
    char buffer[256];
    char _c_r[] = "\0\0";

    while ( fmt[index] )
    {
        switch (fmt[index]){

        case '%':

            ++index;

            switch (fmt[index]){

            case 'c':
                *_c_r = c = (char) va_arg (ap, int);
                str_tmp = _vsputs_r(str_tmp,_c_r);
                break;

            case 's':
                s = va_arg (ap, char*);
                str_tmp = _vsputs_r(str_tmp,s);
                break;

            case 'd':
            case 'i':
                d = va_arg (ap, int);
                kinguio_itoa (d,buffer);
                str_tmp = _vsputs_r(str_tmp,buffer);
                break;

            case 'u':
                u = va_arg (ap, unsigned int);
                kinguio_itoa (u,buffer);
                str_tmp = _vsputs_r(str_tmp,buffer);
                break;

            case 'X':
            case 'x':
                d = va_arg (ap, int);
                kinguio_i2hex(d, buffer,8);
                str_tmp = _vsputs_r(str_tmp,buffer);
                break;

            default:
                str_tmp = _vsputs_r(str_tmp,"%%");
                break;
            }
            break;

        default:
            *_c_r = fmt[index];
            str_tmp = _vsputs_r(str_tmp,_c_r);
            break;
        }
        ++index;
    }

    return (int) ( (long) str_tmp - (long) str );
}


void kinguio_puts(const char* str)
{
    register int i=0;
    size_t StringLen=0;

    if (!str){
        return;
    }

    StringLen = (size_t) strlen(str);

    for (i=0; i<StringLen; i++)
    {
        putchar( str[i] );
    };
}


// printf
// Credits: Nelson Cole. Project Sirius/Kinguio.

int kinguio_printf(const char *fmt, ...)
{
    static char print_buffer[1024];
    int ret=0;

// If the virtual console isn't full initialized yet.
    if( Initialization.console_log != TRUE ){
        return -1;
    }

//----------
    va_list ap;
    va_start(ap, fmt);
    memset(print_buffer, 0, 1024); 
    ret = kinguio_vsprintf(print_buffer, fmt, ap);
    va_end(ap);
//-----------

// Print and return.
    kinguio_puts(print_buffer);
    return (int) ret;
}
// ===================================

int kputs(const char *str)
{
    if ( (void *) str == NULL ){
        return -1;
    }
    return (int) printk ("%s",str);
    //return (int) printf ("%s",str);
}


/*
 * sprintf_old:
 *     @field 2
 *     int sprintf ( char * str, const char * format, ... );
 *     Composes a string with the same text that would be 
 * printed if format was used on printf, 
 * but instead of being printed, the content is stored 
 * as a C string in the buffer pointed by str.
 */
// #suspensa
// Essa implementa????o foi feita para 32bit e n??o funciona
// por inteiro em long mode.

int sprintf_old ( char *str, const char *format, ... )
{
    register int *varg = (int *) (&format);

    return (int) print (&str, varg);
}    


// sprintf
// Variable parameter form to achieve sprintf 
int mysprintf(char *buf, const char *fmt, ...)
{
    int i=0;

// Write the fmt format string to the buffer buf 
    va_list args;
    va_start(args, fmt);
    i = kinguio_vsprintf(buf, fmt, args);
    va_end(args);

    return (int) i;
}


int k_ungetc ( int c, file *f )
{
    if (c == EOF){ 
        return (int) c;
    }

    if ( (void *) f == NULL ){
        return EOF;
    }

// #todo: 
// flag eof.

    //stream->flags = (stream->flags & ~_IOEOF);

    f->_p--;
    f->_p[0] = (char) c;

    return (int) c;
}


long k_ftell(file *f)
{
    if ( (void *) f == NULL ){
        return EOF;
    }
    return (long) (f->_p - f->_base);
}

// Return the fd.
int k_fileno(file *f)
{
    if ( (void *) f == NULL ){
        return EOF;
    }
    return (int) f->_file;  
}


int k_fgetc(file *f)
{
    int ch=0;

    if ( (void *) f == NULL ){
        printf ("k_fgetc: f\n");
        goto fail;
    }

    //(--(p)->_r < 0 ? __srget(p) : (int)(*(p)->_p++))

//#fim.
//cnt decrementou e chegou a zero.
//N??o h?? mais caracteres dispon??veis entre 
    //stream->_ptr e o tamanho do buffer.
		
		/*
		if ( stream->_cnt <= 0 )
		{
			stream->_flags = (stream->_flags | _IOEOF); 
			stream->_cnt = 0;
			
		    //printf ("#debug: fgetc: $\n");
			
			//isso funciona, significa que a estrutura tem ponteiro e base validos.
			//printf("show fgetc:: %s @\n", stream->_base );
		    //refresh_screen();
			
			return EOF;
		};
		*/
		
		//#debug
		//nao podemos acessar um ponteiro nulo... no caso endere??o.


// Buffer pointer
    if ( f->_p == 0 ){
        printf ("k_fgetc: [FAIL] f->_p \n");
        goto fail;
    }

// #obs: 
// Tem que ter a op????o de pegarmos usando o posicionamento
// no buffer. O terminal gosta dessas coisas.

//
// Get the char.
//

    ch = (int) *f->_p;

// Update the pointer
    f->_p++;
    f->_cnt--;

    return (int) ch;

fail:
    //#debug
    printf ("k_fgetc: fail\n");
    refresh_screen();
    return EOF;
}


int k_feof ( file *f )
{
    int ch=0;
 
    if ( (void *) f == NULL ){
        return (int) (-1);
    } else {

        ch = k_fgetc(f);

        if ( ch == EOF ){
             return (int) 1;
        }else{
             return 0;
        };
    };

//checar se o eof foi atingido.
    // return( (stream->_flag & _IOEOF) );

    return 0;
}


int k_ferror(file *f)
{
    if ( (void *) f == NULL ){
        return EOF;
    }

    return (int) ( ( f->_flags & _IOERR ) );
}


// k_fseek:
// offset argument is the position that you want to seek to,
// and whence is what that offset is relative to.
// #todo
// Checar limites do offset.
// #dictioonary-ptbr: whence=deonde
// #todo
// IN:
int k_fseek ( file *f, long offset, int whence )
{
    register int i=0;
    char *p;

    if ( (void *) f == NULL ){
        debug_print ("k_fseek: f\n");
        goto fail;
    }

    if (whence<0){
        goto fail;
    }

// #see: kstdio.h
    switch (whence){


// N??o podemos aceitar valores negativos,
// pois ultrapassa o inicio do arquivo.
// N??o podemos colocar o ponteiro de leitura
// depois do fim do arquivo.
// N??o mexe no offset de escrita.
// continuaremos a escrever de onde paramos.
// Atualiza o offset de leitura.
// A quantidade de bytes dispon??veis
// continua a mesma, pois n??o inclu??mos ou retiramos 
// dados de dentro do buffer.
// O ponteiro de trabalho para refletir a pr??xima
// leitura. mas n??o precisa isso, pois o offset ?? relativo ?? base.
    case SEEK_SET:
        if(offset<0){
            offset=0;
        }
        if( offset > f->_cnt ){
            offset = f->_cnt;
        }
        f->_r = offset;
        f->_p = (f->_base + offset); 
        goto done;
        break;

// Levando em considera????o o ponteiro de trabalho atual,
// vamos mudar o offset e reposicionar o ponteiro de trabalho.
// N??o mexe no offset de escrita.
// continuaremos a escrever de onde paramos.
// A quantidade de bytes dispon??veis
// continua a mesma, pois n??o inclu??mos ou retiramos 
// dados de dentro do buffer.
// Atualiza o offset de leitura.
// mas agora em rela????o ao ponteiro de trabalho e 
// n??o em rela????o ao ponteiro da base.
// temos que respeitar a quantidade disponivel.
    case SEEK_CUR:
        if( offset > f->_cnt ){
            offset = f->_cnt;
        }
        f->_p = (f->_p + offset);
        f->_r = (f->_p - f->_base);
        goto done;
        break;

// #todo: 
// For now we can't accept positive values.
// porque vai ultrapassar o fim do arquivo.
// n??o vamos mexer no ponteiro de escrita,
// Vamos continuar escrevendo de onde paramos,
// pois nenhuma informa????o foi incluida ou retirada do arquivo.
// N??o vamos mexer na quantidade de bytes dispon??vel no buffer,
// pois n??o estamos incluindo informa????o no buffer.
    case SEEK_END:
        if(offset>0){
            offset=0;
        }
        f->_p = f->_base +  (f->_lbfsize + offset);
        f->_r = ( f->_lbfsize + offset );
        goto done;
        break;

// #see: kstdio.h
// BUFSIZ is the minimum for a valid buffer.
// Lets clear only the first part of the buffer.
    case GRAMADO_SEEK_CLEAR:
        p = f->_base;
        //#test
        for (i=0; i<BUFSIZ; i++)
        {
           *p = 0;
           p++; 
        };
        f->_p = f->_base;
        f->_r = 0;
        f->_w = 0;
        f->_cnt = f->_lbfsize;
        goto done;
        break;

// Default
    default:
        //printf ("default:\n");
        goto fail;
        break;
    };

fail:
    printf ("k_fseek fail\n");
    refresh_screen();
    return (int) (-1);
done:
    return 0;
}


// bsd stuff
/*
 * Various output routines call wsetup to be sure it is safe to write,
 * because either _flags does not include __SWR, or _buf is NULL.
 * _wsetup returns 0 if OK to write, nonzero otherwise.
 */
/*
int __swsetup(file *fp)
int __swsetup(file *fp)
{
    return -1;
}
*/


/*
 * Write the given character into the (probably full) buffer for
 * the given file.  Flush the buffer out if it is or becomes full,
 * or if c=='\n' and the file is line buffered.
 */
// #importante
// Isso ?? usado em __sputc no bsd.
/*
int __swbuf (int c, file *fp);
int __swbuf (int c, file *fp)
{
    return -1;
}
*/


int k_fputc ( int ch, file *f )
{
    //debug_print ("k_fputc:\n");

// Pointer validation

    if ( (void *) f == NULL )
    {
        debug_print ("k_fputc: f\n");
        panic       ("k_fputc: f\n");
        return EOF;
    }

// The buffer is not full,
// and the char is not a '\n'.

    if ( f->_cnt > 0 && ch != '\n' )
    {
        // Write it into the buffer.
        sprintf ( f->_p, "%c", ch);

        // Update the pointer.
        f->_p++;

        // Update the offset for the writer.
        f->_w++;

        // Update the counter.
        // How many bytes the buffer still have?
        
        f->_cnt--;
 
        // The buffer is full.
        // #todo: What we need to do now?
        if ( f->_cnt <= 0 )
        {
            debug_print ("k_fputc: [DEBUG] _cnt\n");
            panic       ("k_fputc: [DEBUG] _cnt\n");
            
            f->_cnt = 0;
            
            return EOF;
        }

        f->_flags |= __SRD;  //pode ler.

        return (int) ch;  
    }


// Atualizar as flags quando o buffer est?? cheio.
// se o buffer est?? cheio.

    /*
    //Now writing. 
    if (stream->flags & _IORW)
    {
        stream->flags &= ~_IOREAD;
        stream->flags |= _IOWRITE;
    }
    */

    //File is not writable. 
    //if (!(stream->flags & _IOWRITE))
         //return (EOF);        

//fail:
    return EOF;
}


int k_fscanf (file *f, const char *format, ... )
{
    panic ("k_fscanf: #todo\n");
    return -1;
}


/*
int vfprintf ( file *stream, const char *format, stdio_va_list argptr );
int vfprintf ( file *stream, const char *format, stdio_va_list argptr )
{
}
*/


/*
 * Writes format output of a stdarg argument list to a file.
 */

/* 
int vfprintf(file *stream, const char *format, va_list ap);
int vfprintf(file *stream, const char *format, va_list ap)
{
	int n;             // Characters written. 
	char buffer[1024]; // Buffer.             
	
	// Format string. 
	n = vsprintf(buffer, format, ap);
	
	// Write formated string to file. 
	if (n > 0)
		fputs(buffer, stream);
	
	return (n);
}
*/


void k_rewind(file *f)
{
    //fseek (f, 0L, SEEK_SET);

    if ( (void *) f == NULL ){
        return;
    }

    f->_p = f->_base;
}


// Change f->magic to 4321.
int k_fclose (file *f)
{
    if ( (void *) f == NULL ){
        return EOF;
    }
    if (f->used != TRUE){
        return EOF;
    }
    if (f->magic != 1234){
        return EOF;
    }
    f->used = TRUE;
// Inverted.
    f->magic = 4321;
    return 0;
}


int k_fputs ( const char *str, file *f )
{
    int Size=0;

    if ( (void *) f == NULL ){
        return (int) (-1);
    } 

    Size = (int) strlen (str);
    if ( Size > f->_cnt ){
        return (int) (-1);
    }
    f->_cnt = (int) (f->_cnt - Size);
    sprintf ( f->_p, str );
    f->_p = (f->_p + Size);

    return 0;
}


// k_setbuf:
// see: 
// https://linux.die.net/man/3/setvbuf

void k_setbuf (file *f, char *buf)
{
    if ( (void *) f == NULL ){
        debug_print("k_setbuf: f\n");
        return;
    }

    if ( (void *) buf == NULL ){
        debug_print("k_setbuf: buf\n");
        return;
    }

//#todo
//se o buffer ?? v??lido.

    //if (stream->_bf._base != NULL) 
    //{
        //if (stream->cnt > 0)
            //fflush (stream);

        //free (stream->buf);
    //}

// Udate stream.

    f->_bf._base = buf;
    //stream->_lbfsize = size;        
    // ?? stream->bufmode = mode;

    f->_p = buf;
    // f->_p = f->_bf._base
    
    // ??stream->cnt = 0;

// #todo
// Setup all the buffer elements.
// ...
}


void k_setbuffer (file *f, char *buf, size_t size)
{
  // #todo
  // Check parameters.

    /*
    if ( (void *) f == NULL ){
        debug_print("k_setbuffer: f\n");
        return;
    }


    if ( (void *) buf == NULL ){
        debug_print("k_setbuffer: buf\n");
        return;
    }
    */


    if ( (void *) f == NULL )
    {
        // #todo
        // Maybe we need a message here.
        return;

    }else{

        //#todo
        //se o buffer ?? v??lido.
        
        //if (stream->_bf._base != NULL) 
        //{
            //if (stream->cnt > 0)
                //fflush (stream);
                
            //free (stream->buf);
        //}
        
        // Udate stream.
        f->_bf._base = buf;
        f->_lbfsize = size;        
        // ?? stream->bufmode = mode;

        f->_p = buf;
        // ??stream->cnt = 0;
        //...
    };
}


void k_setlinebuf (file *f)
{
    printf ("k_setlinebuf: #todo\n");

    if ( (void *) f == NULL ){
        return;
    }
}


// #todo
// Not implemented yet.
int 
k_setvbuf (
    file *f, 
    char *buf, 
    int mode, 
    size_t size )
{

    if ( (void *) f == NULL )
    {
        // MSG ?
        return -1;

    }else{

        //#todo
        //se o buffer ?? v??lido.

        //if (f->_bf._base != NULL) 
        //{
            //if (f->cnt > 0)
                //fflush (f);
                
            //free (f->buf);
        //}
        
        // Udate stream.
        f->_bf._base = buf;
        f->_lbfsize = size; 
        // ?? f->bufmode = mode;

        f->_p = buf;
        // ??f->cnt = 0;
        //...
    };


    return 0;
}


// Maybe we can do some operations 
// in a regular file using ioctl.
// EINVAL request or argp is not valid.

int 
regularfile_ioctl ( 
    int fd, 
    unsigned long request, 
    unsigned long arg )
{
    debug_print ("regularfile_ioctl: #todo\n");

    if ( fd < 0 || fd >= OPEN_MAX )
    {
        return (int) (-EBADF);
    }

    return -1;
}


static void __initialize_stdin(void)
{
    int slot=-1;

// stdin
// pega slot em file_table[] para stdin

    slot = get_free_slots_in_the_file_table();
    if ( slot < 0 || 
         slot >= NUMBER_OF_FILES )
    {
        x_panic("__initialize_stdin: slot");
    }
    stdin = (file *) file_table[slot];
    if ( (void*) stdin == NULL ){
        x_panic("__initialize_stdin: stdin");
    }
    stdin->filetable_index = slot;

// fd
    stdin->_file = STDIN_FILENO;  //0
// This is a regular file.
    stdin->____object = ObjectTypeFile;
// sync
    stdin->sync.sender = -1;
    stdin->sync.receiver = -1;
    stdin->sync.can_read    = TRUE;
    stdin->sync.can_write   = FALSE;
    stdin->sync.can_execute = FALSE;
    stdin->sync.can_accept  = FALSE;
    stdin->sync.can_connect = FALSE;
// _flags
    stdin->_flags = (__SWR | __SRD); 

// #bugbug
// Esse buffer est?? sendo usado pelo console.

    stdin->_base     = &prompt[0];    //See: kstdio.h
    stdin->_p        = &prompt[0];
    stdin->_bf._base = stdin->_base;
    stdin->_lbfsize = PROMPT_SIZE; //128; //#todo
    stdin->_r = 0;
    stdin->_w = 0;
    stdin->_cnt = PROMPT_SIZE;
    stdin->_tmpfname = "KSTDIN  TXT";
    stdin->fd_counter = 1;

    // inode support.
    // pega slot em inode_table[] 
    slot = get_free_slots_in_the_inode_table();
    if(slot<0 || slot >=NUMBER_OF_FILES){
        x_panic("__initialize_stdin: [FAIL] stdin inode slot\n");
    }
    stdin->inode = (struct inode_d *) inode_table[slot];
    stdin->inodetable_index = slot;
    if( (void*) stdin->inode == NULL ){
        x_panic("__initialize_stdin: [FAIL] stdin inode struct\n");
    }
    stdin->inode->filestruct_counter = 1; //inicialize

// Copy the name
    memcpy ( 
        (void *)      stdin->inode->path, 
        (const void*) stdin->_tmpfname, 
              sizeof( stdin->inode->path ) );

    stdin->used = TRUE;
    stdin->magic = 1234;
}


static void __initialize_stdout(void)
{
    int slot=-1;

// stdout
// pega slot em file_table[] para stdout

    slot = get_free_slots_in_the_file_table();
    if( slot < 0 || 
        slot >= NUMBER_OF_FILES )
    {
        x_panic("__initialize_stdout: slot");
    }
    stdout = (file *) file_table[slot];
    if ( (void*) stdout == NULL ){
        x_panic("__initialize_stdout: stdout");
    }
    stdout->filetable_index = slot;

// fd
    stdout->_file = STDOUT_FILENO;  //1

// This is a virtual console.
// Configurando a estrutura de stdout.
// This is a virtual console device. Used to output
// directly into the virtual console.
// It is a device.

    stdout->____object = ObjectTypeVirtualConsole; 

// sync
    stdout->sync.sender = -1;
    stdout->sync.receiver = -1;
    stdout->sync.can_read    = TRUE;
    stdout->sync.can_write   = TRUE;
    stdout->sync.can_execute = FALSE;
    stdout->sync.can_accept  = FALSE;
    stdout->sync.can_connect = FALSE;
// _flags
    stdout->_flags = (__SWR | __SRD); 

// buffer
    stdout->_base     = &prompt_out[0];  //See: kstdio.h
    stdout->_p        = &prompt_out[0];
    stdout->_bf._base = stdout->_base;
    stdout->_lbfsize  = PROMPT_SIZE; //128; //#todo
    stdout->_r = 0;
    stdout->_w = 0;
    stdout->_cnt = PROMPT_SIZE;
    stdout->_tmpfname = "KSTDOUT DEV";
    stdout->fd_counter = 1;
    // ...
    // inode support.
    // pega slot em inode_table[] 
    slot = get_free_slots_in_the_inode_table();
    if(slot<0 || slot >=NUMBER_OF_FILES){
        x_panic("__initialize_stdout: stdout inode slot\n");
    }
    stdout->inode = (struct inode_d *) inode_table[slot];
    stdout->inodetable_index = slot;
    if( (void*) stdout->inode == NULL ){
        x_panic("__initialize_stdout: stdout inode struct\n");
    }
    stdout->inode->filestruct_counter = 1; //inicialize

// Copy the name
    memcpy( 
        (void*)       stdout->inode->path, 
        (const void*) stdout->_tmpfname, 
              sizeof( stdout->inode->path ) );

    stdout->used = TRUE;
    stdout->magic = 1234;
}

static void __initialize_stderr(void)
{
    int slot = -1;

// stderr
// pega slot em file_table[] para stderr

    slot = get_free_slots_in_the_file_table();
    if( slot < 0 || 
        slot >= NUMBER_OF_FILES )
    {
        x_panic("__initialize_stderr: slot");
    }
    stderr = (file *) file_table[slot];
    if ( (void*) stderr == NULL ){
        x_panic("__initialize_stderr: stderr");
    }
    stderr->filetable_index = slot;

// fd
    stderr->_file = STDERR_FILENO;  //2
// This is a regular file.
    stderr->____object = ObjectTypeFile;
// sync
    stderr->sync.sender = -1;
    stderr->sync.receiver = -1;
    stderr->sync.can_read    = TRUE;
    stderr->sync.can_write   = TRUE;
    stderr->sync.can_execute = FALSE;
    stderr->sync.can_accept  = FALSE;
    stderr->sync.can_connect = FALSE;
// _flags
    stderr->_flags = (__SWR | __SRD); 
// buffer
    stderr->_base     = &prompt_err[0];  //See: kstdio.h
    stderr->_p        = &prompt_err[0];
    stderr->_bf._base = stderr->_base;
    stderr->_lbfsize  = PROMPT_SIZE; //128; //#todo
    stderr->_r = 0;
    stderr->_w = 0;
    stderr->_cnt = PROMPT_SIZE;
    stderr->_tmpfname = "KSTDERR TXT";
    stderr->fd_counter = 1;
    // ...
    // inode support.
    // pega slot em inode_table[] 
    slot = get_free_slots_in_the_inode_table();
    if(slot<0 || slot >=NUMBER_OF_FILES){
        x_panic("__initialize_stderr: stderr inode slot\n");
    }
    stderr->inode = (struct inode_d *) inode_table[slot];
    stderr->inodetable_index = slot;
    if( (void*) stderr->inode == NULL ){
        x_panic("__initialize_stderr: stderr inode struct\n");
    }
    stderr->inode->filestruct_counter = 1; //inicialize

// Copy the name
    memcpy ( 
        (void*)       stderr->inode->path, 
        (const void*) stderr->_tmpfname, 
              sizeof( stderr->inode->path ) );

    stderr->used = TRUE;
    stderr->magic = 1234;
}


// Os buffers dos arquivos acima.
// prompt[]
// Esses prompts s??o usados como arquivos.
// S??o buffers para as streams.
// See: kstdio.h
static void __clear_prompt_buffers(void)
{
    register int i=0;

    for ( i=0; i<PROMPT_SIZE; i++ )
    {
        prompt[i]     = (char) '\0';
        //prompt_in[i] = (char) '\0';  //#todo
        prompt_out[i] = (char) '\0';
        prompt_err[i] = (char) '\0';
    };

    prompt_pos = 0;
}

// Create n files and put the pointer into the file table.
static void __initialize_file_table(void)
{
    register int i=0;
    file *tmp;

    for (i=0; i<NUMBER_OF_FILES; i++)
    {
        tmp = (void*) kmalloc(sizeof(file));
        
        if ((void*)tmp==NULL){
           x_panic("__initialize_file_table: tmp\n");
        }
        memset( tmp, 0, sizeof(struct file_d) );

        tmp->____object = ObjectTypeFile;  // Regular file.
        tmp->_flags = 0;                   // (__SWR | __SRD); 
        tmp->fd_counter = 0;
        tmp->_tmpfname = NULL;
        //...

        tmp->used = TRUE;
        tmp->magic = 1234;
 
        file_table[i] = (unsigned long) tmp; 
    };
}


// Create n inodes and put the pointers
// into the inode table.
static void __initialize_inode_table(void)
{
    register int i=0;
    struct inode_d *tmp_inode;

    for (i=0; i<32; i++)
    {
        tmp_inode = (void*) kmalloc (sizeof(struct inode_d));
        if ((void*)tmp_inode==NULL){
            x_panic("__initialize_inode_table: tmp_inode\n");
        }
        memset( tmp_inode, 0, sizeof(struct inode_d) );

        tmp_inode->filestruct_counter = 0;
        tmp_inode->path[0] = 0;
        //...

        tmp_inode->used = TRUE;
        tmp_inode->magic = 1234;

        inode_table[i] = (unsigned long) tmp_inode; 
    };
}


// #??
// Maybe we are doing this for the second time.
// Configurando o cursor para todos os consoles.
// Estamos fazendo isso pela segunda vez.
// A primeira foi quando criamos os consoles.
// See:
// tty.h
// console.h
// #bugbug
// Isso ja foi feito antes em VirtualConsole_initialize?
// See console.c
// Vamos refazer de forma personalizada.

static void __initialize_virtual_consoles(void)
{
    register int i=0;
    int cWidth=0;
    int cHeight=0;

// Screen width and height
    if ( gSavedX == 0 || gSavedY == 0 )
    {
        x_panic("__initialize_virtual_consoles: gSavedX gSavedY");
    }

// Char width and height
    cWidth  = get_char_width();
    cHeight = get_char_height();
    
    if ( cWidth == 0 || cHeight == 0 ){
        x_panic ("__initialize_virtual_consoles: cWidth cHeight");
    }

    for (i=0; i<CONSOLETTYS_COUNT_MAX; i++)
    {
        // Make the standard initialization.
        if( CONSOLE_TTYS[i].initialized == FALSE){
                console_init_virtual_console(i);
        }

        // Set some personalized values.
        
        // Cursor.
        CONSOLE_TTYS[i].cursor_x = 0;
        CONSOLE_TTYS[i].cursor_y = 0;
        
        // dc: Full screen
        CONSOLE_TTYS[i].fullscreen_flag = TRUE;
        CONSOLE_TTYS[i].cursor_left   = 0;
        CONSOLE_TTYS[i].cursor_top    = 0;
        CONSOLE_TTYS[i].cursor_right  = (gSavedX/cWidth);
        CONSOLE_TTYS[i].cursor_bottom = (gSavedY/cHeight);
        
        //Let's use the standard colors.
        //CONSOLE_TTYS[i].bg_color = COLOR_BLACK;
        //CONSOLE_TTYS[i].fg_color = COLOR_WHITE;
    };

// The foreground console.
    jobcontrol_switch_console(0);

    // #test
    //set_up_cursor(0,1);
    //console_outbyte('x',fg_console);
    //refresh_screen();
    //while(1){}
}


/*
 * kstdio_initialize:
 *     Inicializando stdio pertencente ao kernel base.
 *     Inicializa as estruturas do fluxo padr??o.
 *     Quem chamou essa inicializa????o ?? Em que hora ??
 * #bugbug: Pelo jeito somente depois dessa inicializa????o ?? que temos mensagens 
 * com printf decentes. Ent??o a inicializa????o do kernel precisa disso.
 * >> precisamos antecipar essa iniciliza????o. Mas ela precisa ser depois da
 * inicializa????o da pagina????o.
 */
// Estamos no kernel base em ring 0.
// Queremos que as streams sejam acess??veis para as rotinas
// da libc em ring3. Para a libc alterar os elementos
// da estrutura.
// #bugbug: Talvez seja poss??vel criar essas estruturas
// em mem??ria compartilhada, usado o alocaro apropriado.
// kmalloc com certeza e ring0.

// In this routine:
// + Initializing the structures for stdin, stdout and stderr

int kstdio_initialize (void)
{
    kstdio_standard_streams_initialized =  FALSE;

// Ja temos suporte a print nesse momento por causa
// das configura????es de console. Mas nessa rotina 
// refaremos as configura????es de console.

    debug_print ("kstdio_initialize: [TODO]\n");
    //printf      ("kstdio_initialize: [TODO]\n");

// ??
// Input mode
// #bugbug
// We have another definition of input mode.
// An global io structure.

    g_inputmode = INPUT_MODE_MULTIPLE_LINES;

    stdio_terminalmode_flag = TRUE;
    stdio_verbosemode_flag  = TRUE;

// Last registered error.
    errno = 0;

// Buffers used by the standard stream.
// Initialize the global file table.
// Initialize the global inode table.
    __clear_prompt_buffers();
    __initialize_file_table();
    __initialize_inode_table();


// Initializing the file pointers
// stdio, stdout and stderr.
// See: kstdio.h

// Create standard stream.
    __initialize_stdin();
    __initialize_stdout();
    __initialize_stderr();

// Virtual console (again)
// We already initialize the structure at the beginning
// of the kernel initialization.
// See: VirtualConsole_initialize() in user/console.c
    __initialize_virtual_consoles();

// Background
// #bugbug
// Estamos fazendo isso pela segunda vez.
// A primeira foi em kmain.

    Background_initialize(COLOR_KERNEL_BACKGROUND);

// done
    kstdio_standard_streams_initialized = TRUE;
    return TRUE;
fail:
    return FALSE;
}


