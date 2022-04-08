#ifndef KLOG_H
#define KLOG_H

/**
 * @brief Number of lines in the buffer. Adjustable, only limited by available
 * memory
 */
#define KLOG_LINES 64

/**
 * @brief Length of a single line in characters
 */
#define KLOG_LINE_SIZE 42

/**
 * @brief Index of the next line to fill
 */
extern unsigned int klog_line_index;

/**
 * @brief Index of the current character in the line
 */
extern unsigned int klog_char_index;

/**
 * @brief Actual buffer, to be traced in uMPS3
 */
extern char klog_buffer[KLOG_LINES][KLOG_LINE_SIZE];

/**
 * @brief Print str to klog
 *
 * @param str String to print
 */
void klog_print(char* str);

/**
 * @brief Print a number in hexadecimal format (best for addresses)
 *
 * @param num Number to display
 */
void klog_print_hex(unsigned int num);

#endif
