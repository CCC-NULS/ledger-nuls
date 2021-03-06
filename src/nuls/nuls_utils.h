#ifndef NULS_UTILS_H
#define NULS_UTILS_H

#include <stdbool.h>
#include <inttypes.h>
#include "nuls_internals.h"
#include "nuls_context.h"
#include "os.h"
#include "../io.h"

/**
 * Gets a big-endian representation of the usable publicKey
 * @param publicKey the raw public key containing both coordinated for the elliptic curve
 * @param encoded result holder
 */
void nuls_compress_publicKey(cx_ecfp_public_key_t WIDE *publicKey, uint8_t *out_encoded);

/**
 * Derive address associated to the specific publicKey.
 * @param publicKey original compressed publicKey
 * @param the chainId.
 * @param the addressType.
 * @param output address in ripemid160 format.
 * @param output address in base58 format.
 * @return number of base58 address bytes
 */
unsigned short nuls_public_key_to_encoded_base58(
        uint8_t WIDE *compressedPublicKey,
        uint16_t chainId,
        uint8_t addressType,
        uint8_t *out_address,
        uint8_t *out_addressBase58);

/**
 * Check if an output owner is a standard send to address script
 * @return true if is a standard send to address script
 */
bool is_send_to_address_script(unsigned char *buffer);

/**
 * Check if an output owner is a standard P2SH script
 * @return true if is a standard P2SH script
 */
bool is_send_to_p2sh_script(unsigned char *buffer);

/**
 * Check if an output owner is a an op return script
 * @return true if is an op return script
 */
bool is_op_return_script(unsigned char *buffer);

/**
 * Check if an address is P2PKH
 * @return true if is a P2PKH address
 */
bool is_p2pkh_addr(uint8_t addr_type);

/**
 * Check if an address is Contract
 * @return true if is a Contract address
 */
bool is_contract_addr(uint8_t addr_type);

/**
 * Check if an address is P2SH
 * @return true if is a P2SH address
 */
bool is_p2sh_addr(uint8_t addr_type);

/**
 * Check if tx is a contract type (100, 101, 102)
 * @return true if is a contract type tx
 */
bool is_contract_tx(uint16_t tx_type);

/**
 * Derive encoded base58 associated to the specific address (chainId + addresstype + ripemid160).
 * @param chainId + addresstype + ripemid160 of compressedpubkey
 * @param the encoded base58 address.
 * @return number of base58 address bytes
 */
unsigned short nuls_address_to_encoded_base58(uint8_t WIDE *address, uint8_t *out_address);

/**
 * Reads the packet for Sign requests (tx and msg), sets the reqContext.
 * @param packet the  buffer of communication packet.
 * @return number of bytes read
 */
uint32_t setReqContextForSign(commPacket_t *packet);

/**
 * Reads the packet for getPubKey requests, sets the reqContext.
 * @param packet the  buffer of communication packet.
 * @return number of bytes read
 */
uint32_t setReqContextForGetPubKey(commPacket_t *packet);

/**
 * Kill Private key and reset all the contexts (reqContext, txContext, commContext, commPacket)
 */
void reset_contexts();

#endif