#include "signTx.h"
#include "../../io.h"
#include "txs/nuls_tx_parser.h"
#include "txs/2_transfer.h"
// #include "./txs/voteTx.h"
// #include "./txs/createMultiSig.h"
// #include "./txs/createSignatureTx.h"
// #include "./txs/registerDelegateTx.h"
#include "../approval.h"
#include "../nuls_helpers.h"
#include "../../ui_utils.h"

#define TX_TYPE_CONSENSUS_REWARD 1
#define TX_TYPE_TRANSFER_TX 2
#define TX_TYPE_SET_ALIAS 3
#define TX_TYPE_REGISTER_CONSENSUS_NODE 4
#define TX_TYPE_JOIN_CONSENSUS 5
#define TX_TYPE_CANCEL_CONSENSUS 6
#define TX_TYPE_YELLOW_CARD 7
#define TX_TYPE_RED_CARD 8
#define TX_TYPE_UNREGISTER_CONSENSUS_NODE 9
#define TX_TYPE_BUSINESS_DATA 10
#define TX_TYPE_CREATE_CONTRACT 100
#define TX_TYPE_CALL_CONTRACT 101
#define TX_TYPE_DELETE_CONTRACT 102
#define TX_TYPE_TRANSFER_CONTRACT 103

typedef void (*tx_parse_fn)();
typedef void (*tx_end_fn)();

tx_parse_fn tx_parse;
tx_end_fn tx_end;
ui_processor_fn ui_processor;
step_processor_fn step_processor;

transaction_context_t txContext;

static unsigned int ui_sign_tx_button(unsigned int button_mask, unsigned int button_mask_counter) {
  switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
      if (currentStep < totalSteps) {
        currentStep = step_processor(currentStep);
        ui_processor(currentStep);
        UX_REDISPLAY();
      } else {
        touch_approve();
      }
      break;

    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
      touch_deny(NULL);
      break;
  }
  return 0;
}

void handleSignTxPacket(commPacket_t *packet, commContext_t *context) {
  // if first packet with signing header
  if ( packet->first ) {
    // Reset sha256 and tx
    cx_sha256_init(&txContext.txHash);

    // IMPORTANT this logic below only works if the first packet contains the needed information (Which it should)
    // Set signing context from first packet and patches the .data and .length by removing header length
    setReqContextForSign(packet);

    // Derive pubKey
    nuls_private_derive_keypair(reqContext.bip32path, reqContext.bip32pathLength,
                                &reqContext.privateKey, &reqContext.publicKey, reqContext.chainCode);
    //Paranoid
    os_memset(&reqContext.privateKey, 0, sizeof(reqContext.privateKey));

    //Gen Compressed PubKey
    nuls_compress_publicKey(&reqContext.publicKey, reqContext.compressedPublicKey);

    //Compressed PubKey -> Address
    nuls_public_key_to_encoded_base58(reqContext.compressedPublicKey, reqContext.chainId,
                                      reqContext.addressVersion, reqContext.address);
    reqContext.address[32] = '\0';

    // fetch transaction type and init txContext for signing
    txContext.type = nuls_read_u16(packet->data, 1, 0);
    txContext.totalTxBytes = reqContext.signableContentLength;
    txContext.tx_parsing_state = BEGINNING;
    txContext.tx_parsing_group = COMMON;
    txContext.bytesRead = 0;
    txContext.saveBufferLength = 0;

    switch (txContext.type) {
      case TX_TYPE_TRANSFER_TX:
        tx_parse = tx_parse_specific_2_transfer;
        tx_end = tx_finalize_2_transfer;
        break;
      default:
        THROW(NOT_SUPPORTED);
    }

  }

  //insert at beginning saveBufferForNextChunk if present
  if(txContext.saveBufferLength > 0) {
    uint8_t tmpBuffer[600];
    os_memmove(tmpBuffer, packet->data, packet->length);
    os_memmove(packet->data, txContext.saveBufferForNextChunk, txContext.saveBufferLength);
    os_memmove(packet->data + txContext.saveBufferLength, packet->data, packet->length);
    packet->length += txContext.saveBufferLength;
    txContext.saveBufferLength = 0;
  }

  txContext.bufferPointer = packet->data;
  txContext.bytesChunkRemaining = packet->length;

  BEGIN_TRY {
      TRY {
          switch(txContext.tx_parsing_group) {
            case COMMON:
              parse_group_common();
            case TX_SPECIFIC:
              if(txContext.tx_parsing_group != TX_SPECIFIC) {
                THROW(INVALID_STATE);
              }
              tx_parse();
            case COIN_INPUT:
              parse_group_coin_input();
            case COIN_OUTPUT:
              parse_group_coin_output();
            case CHECK_SANITY_BEFORE_SIGN:
              check_sanity_before_sign();
            default:
              THROW(INVALID_STATE);
          }
        }
      CATCH_OTHER(e) {
          if(e == NEED_NEXT_CHUNK) {
            os_memmove(txContext.saveBufferForNextChunk, txContext.bufferPointer, txContext.bytesChunkRemaining);
            txContext.saveBufferLength = txContext.bytesChunkRemaining;
          } else {
            //Unexpected Error during parsing. Let the client know
            THROW(e);
          }
        }
      FINALLY {
      }
    }
  END_TRY;
}

static uint8_t default_step_processor(uint8_t cur) {
  return cur + 1;
}


void finalizeSignTx(volatile unsigned int *flags) {

  if(txContext.tx_parsing_group != TX_PARSED || txContext.tx_parsing_state != READY_TO_SIGN)
    THROW(INVALID_STATE);

  // Close sha256 and hash again
  cx_hash_finalize(reqContext.digest, DIGEST_LENGTH);

  // Init user flow.
  step_processor = default_step_processor;
  ui_processor = NULL;

  tx_end();

  currentStep = 1;
  *flags |= IO_ASYNCH_REPLY;

  ux.button_push_handler = ui_sign_tx_button;
  ux.elements_preprocessor = uiprocessor;

  ui_processor(1);
  UX_WAKE_UP();
  UX_REDISPLAY();
}
