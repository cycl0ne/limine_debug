#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

// Conversion table for hexadecimal representation
static const char CONVERSION_TABLE[] = "0123456789abcdef";

static inline void outportb(unsigned short _port, unsigned char _data) {
	asm volatile ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

void init_print() {
    outportb(0x3f8 + 3, 0x03); /* Disable divisor mode, set parity */
}	

static inline void putc(char c) {
    outportb(0x3f8, c);
}

static int cb_printf(void * user, char c) {
	putc(c);
	return 0;
}

#define OUT(c) do { callback(userData, (c)); written++; } while (0)

static size_t print_dec(unsigned long long value, unsigned int width, int (*callback)(void*,char), void * userData, int fill_zero, int align_right, int precision) {
	size_t written = 0;
	unsigned long long n_width = 1;
	unsigned long long i = 9;
	if (precision == -1) precision = 1;

	if (value == 0) {
		n_width = 0;
	} else {
		unsigned long long val = value;
		while (val >= 10UL) {
			val /= 10UL;
			n_width++;
		}
	}

	if (n_width < (unsigned long long)precision) n_width = precision;

	int printed = 0;
	if (align_right) {
		while (n_width + printed < width) {
			OUT(fill_zero ? '0' : ' ');
			printed += 1;
		}

		i = n_width;
		char tmp[100];
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			tmp[i - 1] = r + '0';
			i--;
			value = n;
		}
		while (i < n_width) {
			OUT(tmp[i]);
			i++;
		}
	} else {
		i = n_width;
		char tmp[100];
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			tmp[i - 1] = r + '0';
			i--;
			value = n;
			printed++;
		}
		while (i < n_width) {
			OUT(tmp[i]);
			i++;
		}
		while (printed < (long long)width) {
			OUT(fill_zero ? '0' : ' ');
			printed += 1;
		}
	}

	return written;
}

/*
 * Hexadecimal to string
 */
static size_t print_hex(unsigned long long value, unsigned int width, int (*callback)(void*,char), void* userData, int fill_zero, int alt, int caps, int align) {
	size_t written = 0;
	int i = width;

	unsigned long long n_width = 1;
	unsigned long long j = 0x0F;
	while (value > j && j < UINT64_MAX) {
		n_width += 1;
		j *= 0x10;
		j += 0x0F;
	}

	if (!fill_zero && align == 1) {
		while (i > (long long)n_width + 2*!!alt) {
			OUT(' ');
			i--;
		}
	}

	if (alt) {
		OUT('0');
		OUT(caps ? 'X' : 'x');
	}

	if (fill_zero && align == 1) {
		while (i > (long long)n_width + 2*!!alt) {
			OUT('0');
			i--;
		}
	}

	i = (long long)n_width;
	while (i-- > 0) {
		char c = (caps ? "0123456789ABCDEF" : "0123456789abcdef")[(value>>(i*4))&0xF];
		OUT(c);
	}

	if (align == 0) {
		i = width;
		while (i > (long long)n_width + 2*!!alt) {
			OUT(' ');
			i--;
		}
	}

	return written;
}

/*
 * vasprintf()
 */
size_t xvasprintf(int (*callback)(void *, char), void * userData, const char * fmt, va_list args) {
	const char * s;
	size_t written = 0;
	for (const char *f = fmt; *f; f++) {
		if (*f != '%') {
			OUT(*f);
			continue;
		}
		++f;
		unsigned int arg_width = 0;
		int align = 1; /* right */
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
		if (*f == 'j') {
			big = (sizeof(uintmax_t) == sizeof(unsigned long long) ? 2 :
				   sizeof(uintmax_t) == sizeof(unsigned long) ? 1 : 0);
			++f;
		}
		if (*f == 'z') {
			big = (sizeof(size_t) == sizeof(unsigned long long) ? 2 :
				   sizeof(size_t) == sizeof(unsigned long) ? 1 : 0);
			++f;
		}
		if (*f == 't') {
			big = (sizeof(ptrdiff_t) == sizeof(unsigned long long) ? 2 :
				   sizeof(ptrdiff_t) == sizeof(unsigned long) ? 1 : 0);
			++f;
		}
		/* fmt[i] == '%' */
		switch (*f) {
			case 's': /* String pointer -> String */
				{
					size_t count = 0;
					if (big) {
						return written;
					} else {
						s = (char *)va_arg(args, char *);
						if (s == NULL) {
							s = "(null)";
						}
						if (precision >= 0) {
							while (*s && precision > 0) {
								OUT(*s++);
								count++;
								precision--;
								if (arg_width && count == arg_width) break;
							}
						} else {
							while (*s) {
								OUT(*s++);
								count++;
								if (arg_width && count == arg_width) break;
							}
						}
					}
					while (count < arg_width) {
						OUT(' ');
						count++;
					}
				}
				break;
			case 'c': /* Single character */
				OUT((char)va_arg(args,int));
				break;
			case 'p':
				alt = 1;
				if (sizeof(void*) == sizeof(long long)) big = 2;
				/* fallthrough */
			case 'X':
			case 'x': /* Hexadecimal number */
				{
					unsigned long long val;
					if (big == 2) {
						val = (unsigned long long)va_arg(args, unsigned long long);
					} else if (big == 1) {
						val = (unsigned long)va_arg(args, unsigned long);
					} else {
						val = (unsigned int)va_arg(args, unsigned int);
					}
					written += print_hex(val, arg_width, callback, userData, fill_zero, alt, !(*f & 32), align);
				}
				break;
			case 'i':
			case 'd': /* Decimal number */
				{
					long long val;
					if (big == 2) {
						val = (long long)va_arg(args, long long);
					} else if (big == 1) {
						val = (long)va_arg(args, long);
					} else {
						val = (int)va_arg(args, int);
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
			case 'u': /* Unsigned ecimal number */
				{
					unsigned long long val;
					if (big == 2) {
						val = (unsigned long long)va_arg(args, unsigned long long);
					} else if (big == 1) {
						val = (unsigned long)va_arg(args, unsigned long);
					} else {
						val = (unsigned int)va_arg(args, unsigned int);
					}
					written += print_dec(val, arg_width, callback, userData, fill_zero, align, precision);
				}
				break;
			case '%': /* Escape */
				OUT('%');
				break;
			default: /* Nothing at all, just dump it */
				OUT(*f);
				break;
		}
	}
	return written;
}


int printf(const char * fmt, ...) 
{
	va_list args;
	va_start(args, fmt);
	int out = xvasprintf(cb_printf, NULL, fmt, args);
	va_end(args);
	return out;
}

#if 0
// Outputs a single character to the I/O port 0x3f8.
// This is typically used for low-level debugging (e.g., with QEMU).
static inline void putc(char c) {
    outportb(0x3f8, c);
}

// Outputs a null-terminated string by sending each character via putc.
static void print(const char *msg) {
    for (size_t i = 0; msg[i] != '\0'; i++) {
        putc(msg[i]);
    }
}

// Outputs a null-terminated string followed by a newline character.
void puts(const char *msg) {
    print(msg);
    putc('\n');
}

// Converts and prints a number in hexadecimal format.
static void printhex(size_t num) {
    char buf[17]; // Buffer to hold hexadecimal digits plus null terminator
    int i = 16;

    buf[16] = '\0'; // Null-terminate the buffer

    if (num == 0) {
        print("0x0");
        return;
    }

    // Convert number to hex digits, starting from the end of the buffer
    while (num != 0 && i > 0) {
        buf[--i] = CONVERSION_TABLE[num % 16];
        num /= 16;
    }

    print("0x");
    print(&buf[i]);
}

// Converts and prints a number in decimal format.
static void printdec(size_t num) {
    char buf[21] = {0}; // Buffer to hold decimal digits (up to 20 digits for 64-bit) plus null terminator
    int i = 20;

    if (num == 0) {
        putc('0');
        return;
    }

    // Convert number to decimal digits, starting from the end of the buffer
    while (num != 0 && i > 0) {
        buf[--i] = '0' + (num % 10);
        num /= 10;
    }

    print(&buf[i]);
}

// A simplified printf function supporting %x, %d, and %s format specifiers.
void printf(const char *format, ...) {
    va_list argp;
    va_start(argp, format);

    while (*format != '\0') {
        if (*format == '%') {
            format++; // Move past '%'
            switch (*format) {
                case 'x': {
                    size_t num = va_arg(argp, size_t);
                    printhex(num);
                    break;
                }
                case 'd': {
                    size_t num = va_arg(argp, size_t);
                    printdec(num);
                    break;
                }
                case 's': {
                    const char *str = va_arg(argp, const char *);
                    if (str != NULL) {
                        print(str);
                    } else {
                        print("(null)");
                    }
                    break;
                }
                case '%': { // Handle escaped '%%'
                    putc('%');
                    break;
                }
                default: { // Handle unknown specifiers by printing them as-is
                    putc('%');
                    putc(*format);
                    break;
                }
            }
        } else {
            putc(*format);
        }
        format++;
    }

    va_end(argp);
}
#endif