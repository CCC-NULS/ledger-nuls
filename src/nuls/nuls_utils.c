#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include "nuls_utils.h"
#include "os.h"
#include "cx.h"

const unsigned char TX_OUTPUT_SCRIPT_ADDRESS_PRE[] = {
        0x19, 0x76, 0xA9,
        0x17}; // script length, OP_DUP, OP_HASH160, address length
const unsigned char TX_OUTPUT_SCRIPT_ADDRESS_POST[] = {
        0x88, 0xAC}; // OP_EQUALVERIFY, OP_CHECKSIG

const unsigned char TX_OUTPUT_SCRIPT_P2SH_PRE[] = {
        0x1A, 0xA9, 0x17}; // script length, OP_HASH160, address length
const unsigned char TX_OUTPUT_SCRIPT_P2SH_POST[] = {0x87}; // OP_EQUAL


void nuls_compress_publicKey(cx_ecfp_public_key_t WIDE *publicKey, uint8_t *out_encoded) {
  os_memmove(out_encoded, publicKey->W, 33);
  out_encoded[0] = ((publicKey->W[64] & 1) ? 0x03 : 0x02);
}

void nuls_public_key_hash160(unsigned char *in, unsigned short inlen, unsigned char *out) {
  union {
      cx_sha256_t shasha;
      cx_ripemd160_t riprip;
  } u;
  unsigned char buffer[32];

  cx_sha256_init(&u.shasha);
  cx_hash(&u.shasha.header, CX_LAST, in, inlen, buffer, 32);
  cx_ripemd160_init(&u.riprip);
  cx_hash(&u.riprip.header, CX_LAST, buffer, 32, out, 20);
}

uint8_t getxor(uint8_t *buffer, uint8_t length) {
  uint8_t xor = 0;
  for (int i = 0; i < length; i++) {
    xor = xor ^ buffer[i];
  }
  return xor;
}

bool is_send_to_address_script(unsigned char *buffer) {
    if ((os_memcmp(buffer, TX_OUTPUT_SCRIPT_ADDRESS_PRE,
                   sizeof(TX_OUTPUT_SCRIPT_ADDRESS_PRE)) == 0) &&
        (os_memcmp(buffer + sizeof(TX_OUTPUT_SCRIPT_ADDRESS_PRE) + ADDRESS_LENGTH,
                   TX_OUTPUT_SCRIPT_ADDRESS_POST,
                   sizeof(TX_OUTPUT_SCRIPT_ADDRESS_POST)) == 0)) {
        return true;
    }
    return false;
}

bool is_send_to_p2sh_script(unsigned char *buffer) {
    if ((os_memcmp(buffer, TX_OUTPUT_SCRIPT_P2SH_PRE,
                   sizeof(TX_OUTPUT_SCRIPT_P2SH_PRE)) == 0) &&
        (os_memcmp(buffer + sizeof(TX_OUTPUT_SCRIPT_P2SH_PRE) + ADDRESS_LENGTH,
                   TX_OUTPUT_SCRIPT_P2SH_POST,
                   sizeof(TX_OUTPUT_SCRIPT_P2SH_POST)) == 0)) {
        return true;
    }
    return false;
}

bool is_op_return_script(unsigned char *buffer) {
    return (buffer[1] == 0x6A); //OP_RETURN
}


bool is_p2pkh_addr(uint8_t addr_type) {
  return addr_type == ADDRESS_TYPE_P2PKH;
}

bool is_contract_addr(uint8_t addr_type) {
  return addr_type == ADDRESS_TYPE_CONTRACT;
}

bool is_p2sh_addr(uint8_t addr_type) {
  return addr_type == ADDRESS_TYPE_P2SH;
}

bool is_contract_tx(uint16_t tx_type) {
  return  tx_type == TX_TYPE_100_CREATE_CONTRACT ||
          tx_type == TX_TYPE_101_CALL_CONTRACT ||
          tx_type == TX_TYPE_102_DELETE_CONTRACT ||
          tx_type == TX_TYPE_103_TRANSFER_CONTRACT;
}

unsigned short nuls_public_key_to_encoded_base58 (
        uint8_t WIDE *compressedPublicKey,
        uint16_t chainId, uint8_t addressType,
        uint8_t *out_address,
        uint8_t *out_addressBase58) {
  out_address[0] = chainId;
  out_address[1] = (chainId >> 8);
  out_address[2] = addressType;
  nuls_public_key_hash160(compressedPublicKey, 33, out_address + 3);
  return nuls_address_to_encoded_base58(out_address, out_addressBase58);
}

unsigned short nuls_address_to_encoded_base58(
        uint8_t WIDE *nulsRipemid160, //chainId + addresstype + ripemid160
        uint8_t *out_addressBase58) {
  unsigned char tmpBuffer[24];
  os_memmove(tmpBuffer, nulsRipemid160, 23);
  tmpBuffer[23] = getxor(tmpBuffer, 23);
  size_t outputLen = 32;
  if (nuls_encode_base58(tmpBuffer, 24, out_addressBase58, &outputLen) < 0) {
    THROW(EXCEPTION);
  }
  return outputLen;
}

void deriveAccountAddress(local_address_t *account) {

  // Derive pubKey
  nuls_private_derive_keypair(account->path, account->pathLength, account->chainCode);
  //Paranoid
  os_memset(&private_key, 0, sizeof(private_key));
  //Gen Compressed PubKey
  nuls_compress_publicKey(&public_key, account->compressedPublicKey);
  //Compressed PubKey -> Address
  nuls_public_key_to_encoded_base58(account->compressedPublicKey, account->chainId,
                                    account->type, account->address, account->addressBase58);
  account->addressBase58[32] = '\0';
}

uint32_t extractAccountInfo(uint8_t *data, local_address_t *account) {
  uint32_t readCounter = 0;

  //PathLength
  account->pathLength = data[0];
  readCounter++;

  if(account->pathLength == 0) {
    return readCounter;
  }

  if(account->pathLength > MAX_BIP32_PATH) {
    THROW(INVALID_PARAMETER);
  }

  //AddressType
  account->type = data[1];
  readCounter++;

  //At the moment P2SH is not supported
  if(account->type != 0x01) {
    THROW(NOT_SUPPORTED);
  }

  //Path
  nuls_bip32_buffer_to_array(data + 2, account->pathLength, account->path);
  readCounter += account->pathLength * 4;

  //Set chainId from account->path[1]
  account->chainId = (uint16_t) account->path[1]^0x80000000;

  deriveAccountAddress(account);

  return readCounter;
}

uint32_t setReqContextForSign(commPacket_t *packet) {
  reqContext.signableContentLength = 0;
  uint32_t headerBytesRead = 0;

  //AccountFrom
  headerBytesRead += extractAccountInfo(packet->data, &(reqContext.accountFrom));

  //AccountChange
  headerBytesRead += extractAccountInfo(packet->data + headerBytesRead, &(reqContext.accountChange));

  //Check chainId is the same if (any) change address
  if(reqContext.accountChange.pathLength > 0 && (reqContext.accountChange.chainId != reqContext.accountFrom.chainId)) {
    THROW(INVALID_PARAMETER);
  }

  //Data Length
  reqContext.signableContentLength = nuls_read_u32(packet->data + headerBytesRead, 1, 0);
  headerBytesRead += 4;
  // Check signable content length if is correct
  if (reqContext.signableContentLength >= commContext.totalAmount) {
    THROW(0x6700); // INCORRECT_LENGTH
  }

  return headerBytesRead;
}

uint32_t setReqContextForGetPubKey(commPacket_t *packet) {
  reqContext.showConfirmation = packet->data[0];
  return extractAccountInfo(packet->data + 1, &reqContext.accountFrom);
}

void reset_contexts() {
  // Kill private key - shouldn't be necessary but just in case.
  os_memset(&private_key, 0, sizeof(private_key));

  os_memset(&reqContext, 0, sizeof(reqContext));
  os_memset(&txContext, 0, sizeof(txContext));
  os_memset(&commContext, 0, sizeof(commContext));
  os_memset(&commPacket, 0, sizeof(commPacket));

  // Allow restart of operation
  commContext.started = false;
  commContext.read = 0;
}