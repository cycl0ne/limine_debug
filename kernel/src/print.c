#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

static bool initPrint = false;

/* Write a byte to the given I/O port */
static inline void outportb(uint16_t _port, uint8_t _data) {
    asm volatile ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

/* Initialize serial port communication for printing */
void init_print() {
    outportb(0x3f8 + 3, 0x03); /* Set serial line to 8 bits, no parity, one stop bit */
    initPrint = true;
}

/* Callback function for character output used in formatted printing */
static int cb_printf(void * user, char c) {
    user = user;
    if (initPrint) outportb(0x3f8, c);
    return 1;
}

#define OUT(c) do { callback(userData, (c)); written++; } while (0)

/* Print a decimal value to the serial port */
static size_t print_dec(uint64_t value, uint32_t width, int (*callback)(void*, char), void * userData, int fill_zero, int align_right, int precision) {
    size_t written = 0;
    uint64_t n_width = 1;
    if (precision == -1) precision = 1;

    if (value == 0) {
        n_width = 0;
    } else {
        uint64_t val = value;
        while (val >= 10UL) {
            val /= 10UL;
            n_width++;
        }
    }

    if (n_width < (uint64_t)precision) n_width = precision;

    int printed = 0;
    if (align_right) {
        while (n_width + printed < width) {
            OUT(fill_zero ? '0' : ' ');
            printed += 1;
        }

        char tmp[100];
        for (int i = n_width - 1; i >= 0; i--) {
            tmp[i] = (value % 10) + '0';
            value /= 10;
        }
        for (int i = 0; i < n_width; i++) {
            OUT(tmp[i]);
        }
    } else {
        char tmp[100];
        for (int i = n_width - 1; i >= 0; i--) {
            tmp[i] = (value % 10) + '0';
            value /= 10;
        }
        for (int i = 0; i < n_width; i++) {
            OUT(tmp[i]);
        }
        while (printed < (int64_t)width) {
            OUT(fill_zero ? '0' : ' ');
            printed += 1;
        }
    }

    return written;
}

/* Print a hexadecimal value to the serial port */
static size_t print_hex(uint64_t value, uint32_t width, int (*callback)(void*, char), void* userData, int fill_zero, int alt, int caps, int align) {
    size_t written = 0;
    uint64_t n_width = 1;
    uint64_t j = 0x0F;
    while (value > j && j < UINT64_MAX) {
        n_width += 1;
        j *= 0x10;
        j += 0x0F;
    }

    if (!fill_zero && align == 1) {
        while (width > (int64_t)n_width + 2 * !!alt) {
            OUT(' ');
            width--;
        }
    }

    if (alt) {
        OUT('0');
        OUT(caps ? 'X' : 'x');
    }

    if (fill_zero && align == 1) {
        while (width > (int64_t)n_width + 2 * !!alt) {
            OUT('0');
            width--;
        }
    }

    for (int i = n_width - 1; i >= 0; i--) {
        char c = (caps ? "0123456789ABCDEF" : "0123456789abcdef")[(value >> (i * 4)) & 0xF];
        OUT(c);
    }

    if (align == 0) {
        while (width > (int64_t)n_width + 2 * !!alt) {
            OUT(' ');
            width--;
        }
    }

    return written;
}

/* Formatted string printing function */
size_t xvasprintf(int (*callback)(void *, char), void * userData, const char * fmt, va_list args) {
    size_t written = 0;
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            OUT(*f);
            continue;
        }
        ++f;
        uint32_t arg_width = 0;
        int align = 1;
        int fill_zero = 0;
        int big = 0;
        int alt = 0;
        int always_sign = 0;
        int precision = -1;

        while (1) {
            if (*f == '-') {
                align = 0;
                ++f;
            } else if (*f == '#') {
                alt = 1;
                ++f;
            } else if (*f == '*') {
                arg_width = (char)va_arg(args, int);
                ++f;
            } else if (*f == '0') {
                fill_zero = 1;
                ++f;
            } else if (*f == '+') {
                always_sign = 1;
                ++f;
            } else if (*f == ' ') {
                always_sign = 2;
                ++f;
            } else {
                break;
            }
        }
        while (*f >= '0' && *f <= '9') {
            arg_width *= 10;
            arg_width += *f - '0';
            ++f;
        }
        if (*f == '.') {
            ++f;
            precision = 0;
            if (*f == '*') {
                precision = (int)va_arg(args, int);
                ++f;
            } else  {
                while (*f >= '0' && *f <= '9') {
                    precision *= 10;
                    precision += *f - '0';
                    ++f;
                }
            }
        }
        if (*f == 'l') {
            big = 1;
            ++f;
            if (*f == 'l') {
                big = 2;
                ++f;
            }
        }
        /* fmt[i] == '%' */
        switch (*f) {
            case 's':
                {
                    const char *s = va_arg(args, char *);
                    if (s == NULL) s = "(null)";
                    size_t count = 0;
                    while (*s && (precision < 0 || count < (size_t)precision)) {
                        OUT(*s++);
                        count++;
                    }
                    while (count < arg_width) {
                        OUT(' ');
                        count++;
                    }
                }
                break;
            case 'c':
                OUT((char)va_arg(args, int));
                break;
            case 'x':
            case 'X':
                {
                    uint64_t val;
                    if (big == 2) {
                        val = va_arg(args, uint64_t);
                    } else if (big == 1) {
                        val = va_arg(args, unsigned long);
                    } else {
                        val = va_arg(args, unsigned int);
                    }
                    written += print_hex(val, arg_width, callback, userData, fill_zero, alt, *f == 'X', align);
                }
                break;
            case 'd':
            case 'i':
                {
                    int64_t val;
                    if (big == 2) {
                        val = va_arg(args, int64_t);
                    } else if (big == 1) {
                        val = va_arg(args, long);
                    } else {
                        val = va_arg(args, int);
                    }
                    if (val < 0) {
                        OUT('-');
                        val = -val;
                    } else if (always_sign) {
                        OUT(always_sign == 2 ? ' ' : '+');
                    }
                    written += print_dec(val, arg_width, callback, userData, fill_zero, align, precision);
                }
                break;
            case 'u':
                {
                    uint64_t val;
                    if (big == 2) {
                        val = va_arg(args, uint64_t);
                    } else if (big == 1) {
                        val = va_arg(args, unsigned long);
                    } else {
                        val = va_arg(args, unsigned int);
                    }
                    written += print_dec(val, arg_width, callback, userData, fill_zero, align, precision);
                }
                break;
            case '%':
                OUT('%');
                break;
            default:
                OUT(*f);
                break;
        }
    }
    return written;
}

/* Print formatted string to the serial port */
int printf(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int out = xvasprintf(cb_printf, NULL, fmt, args);
    va_end(args);
    return out;
}
