#ifndef UTILS_H
#define UTILS_H

#include <QString>


/**
 * @brief byteString2BinaryString - Convert byte string stream to binary string stream.
 * For example, 68       04       07       00       00       00      (byte string)
 *              01101000 00000100 00000111 00000000 00000000 00000000(binary string)
 * @param s - The string to be converted which has even bytes.
 * @param binary - binary should have enough space.
 * @return
 */
void byteString2BinaryString(QString s, uint8_t binary[]);



#endif // UTILS_H
