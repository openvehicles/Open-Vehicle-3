/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "mcp2515";

#include <string.h>
#include "mcp2515.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_intr.h"
#include "soc/dport_reg.h"

static void MCP2515_isr(void *pvParameters)
  {
  mcp2515 *me = (mcp2515*)pvParameters;

  CAN_msg_t msg;
  msg.type = CAN_rxcallback;
  msg.body.bus = me;

  //send callback request to main CAN processor task
  xQueueSendFromISR(MyCan.m_rxqueue,&msg,0);
  }

mcp2515::mcp2515(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin)
  : canbus(name)
  {
  m_spibus = spibus;
  m_host = host;
  m_clockspeed = clockspeed;
  m_cspin = cspin;
  m_intpin = intpin;

  memset(&m_devcfg, 0, sizeof(spi_nodma_device_interface_config_t));
  m_devcfg.clock_speed_hz=m_clockspeed;     // Clock speed (in hz)
  m_devcfg.mode=0;                          // SPI mode 0
  m_devcfg.command_bits=0;
  m_devcfg.address_bits=0;
  m_devcfg.dummy_bits=0;
  m_devcfg.spics_io_num=m_cspin;
  m_devcfg.queue_size=7;                    // We want to be able to queue 7 transactions at a time

  esp_err_t ret = spi_nodma_bus_add_device(m_host, &m_spibus->m_buscfg, &m_devcfg, &m_spi);
  assert(ret==ESP_OK);

  gpio_set_intr_type((gpio_num_t)m_intpin, GPIO_INTR_NEGEDGE);
  gpio_isr_handler_add((gpio_num_t)m_intpin, MCP2515_isr, (void*)this);

  // Initialise in powered down mode
  m_powermode = Off; // Stop an event being raised
  SetPowerMode(Off);
  }

mcp2515::~mcp2515()
  {
  gpio_isr_handler_remove((gpio_num_t)m_intpin);
  }

esp_err_t mcp2515::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  canbus::Start(mode, speed);
  uint8_t buf[16];

  m_mode = mode;
  m_speed = speed;

  // RESET commmand
  m_spibus->spi_cmd(m_spi, buf, 0, 1, CMD_RESET);
  vTaskDelay(50 / portTICK_PERIOD_MS);

  // Set CONFIG mode (abort transmisions, one-shot mode, clkout disabled)
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x0f, 0b10011000);
  vTaskDelay(50 / portTICK_PERIOD_MS);

  // Rx Buffer 0 control (receive all and enable buffer 1 rollover)
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x60, 0b01100100);

  // CANINTE (interrupt enable), all interrupts
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x2b, 0b11111111);

  // BFPCTRL RXnBF PIN CONTROL AND STATUS
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x0c,  0b00001100);

  // Bus speed
  uint8_t cnf1 = 0;
  uint8_t cnf2 = 0;
  uint8_t cnf3 = 0;
  switch (m_speed)
    {
    case CAN_SPEED_100KBPS:
      cnf1=0x03; cnf2=0xfa; cnf3=0x87;
      break;
    case CAN_SPEED_125KBPS:
      cnf1=0x03; cnf2=0xf0; cnf3=0x86;
      break;
    case CAN_SPEED_250KBPS:
      cnf1=0x41; cnf2=0xf1; cnf3=0x85;
      break;
    case CAN_SPEED_500KBPS:
      cnf1=0x00; cnf2=0xf0; cnf3=0x86;
      break;
    case CAN_SPEED_1000KBPS:
      cnf1=0x00; cnf2=0xd0; cnf3=0x82;
      break;
    }
  m_spibus->spi_cmd(m_spi, buf, 0, 5, CMD_WRITE, 0x28, cnf3, cnf2, cnf1);

  // Set NORMAL mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x0f, 0x00);

  // And record that we are powered on
  pcp::SetPowerMode(On);

  return ESP_OK;
  }

esp_err_t mcp2515::Stop()
  {
  uint8_t buf[16];

  // RESET command
  m_spibus->spi_cmd(m_spi, buf, 0, 1, CMD_RESET);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // BFPCTRL RXnBF PIN CONTROL AND STATUS
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x0c, 0b00111100);

  // Set SLEEP mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, 0x0f, 0x30);

  // And record that we are powered down
  pcp::SetPowerMode(Off);

  return ESP_OK;
  }

esp_err_t mcp2515::Write(const CAN_frame_t* p_frame)
  {
  canbus::Write(p_frame);
  uint8_t buf[16];
  uint8_t id[4];

  // check for free TX buffer:
  uint8_t txbuf;
  uint8_t* p = m_spibus->spi_cmd(m_spi, buf, 1, 1, CMD_READ_STATUS);
  if ((p[0] & 0b00000100) == 0)
    txbuf = 0b000; // use TXB0
  else if ((p[0] & 0b00010000) == 0)
    txbuf = 0b010; // use TXB1
  else if ((p[0] & 0b01000000) == 0)
    txbuf = 0b100; // use TXB2
  else
    {
    m_errors_txbuf_overflow++;
    return ESP_FAIL;
    }
  
  if (p_frame->FIR.B.FF == CAN_frame_std)
    {
    // Transmit a standard frame
    id[0] = p_frame->MsgID >> 3;     // HIGH 8 bits of standard ID
    id[1] = p_frame->MsgID << 5;     // LOW 3 bits of standard ID
    id[2] = 0;
    id[3] = 0;
    }
  else
    {
    // Transmit an extended frame
    id[0] = (p_frame->MsgID >> 21) & 0xff;    // HIGH bits
    id[1] = ((p_frame->MsgID >> 13) & 0xe0)   // Next middle bits of extended ID; set the EXT bit too.
          + ((p_frame->MsgID >> 16) & 0x03)
          + 0x08;
    id[2] = (p_frame->MsgID >> 8) & 0xff;     // MID 8 bits of extended ID
    id[3] = (p_frame->MsgID & 0xff);          // LOW 8 bits of extended ID
    }

  // MCP2515 load transmit buffer:
  m_spibus->spi_cmd(m_spi, buf, 0, 14, CMD_LOAD_TXBUF | txbuf,
    id[0], id[1], id[2], id[3], p_frame->FIR.B.DLC,
    p_frame->data.u8[0],
    p_frame->data.u8[1],
    p_frame->data.u8[2],
    p_frame->data.u8[3],
    p_frame->data.u8[4],
    p_frame->data.u8[5],
    p_frame->data.u8[6],
    p_frame->data.u8[7]);

  // MCP2515 request to send:
  m_spibus->spi_cmd(m_spi, buf, 0, 1, CMD_RTS | (txbuf ? txbuf : 0b001));

  return ESP_OK;
  }

bool mcp2515::RxCallback(CAN_frame_t* frame)
  {
  uint8_t buf[16];

  // read interrupts (CANINTF 0x2c) and errors (EFLG 0x2d):
  uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 2, 2, CMD_READ, 0x2c);
  uint8_t intstat = p[0];
  uint8_t errflag = p[1];
  
  // handle RX buffers and other interrupts sequentially:
  int intflag;
  if (intstat & 0x01)
    {
    // RX buffer 0 is full, handle it
    intflag = 0x01;
    }
  else if (intstat & 0x02)
    {
    // RX buffer 1 is full, handle it
    intflag = 0x02;
    }
  else
    {
    // other interrupts:
    intflag = intstat & 0b11111100;
    }
    
  if (intflag == 0)
    {
    // all interrupts handled
    return false;
    }
    
  if (intflag <= 2)
    {
    // The indicated RX buffer has a message to be read
    memset(frame,0,sizeof(*frame));
    frame->origin = this;
    
    // read RX buffer and clear interrupt flag:
    uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 13, 1, CMD_READ_RXBUF + ((intflag==1) ? 0 : 4));
    
    if (p[1] & 0x08) //check for extended mode=1, or std mode=0
      {
      frame->FIR.B.FF = CAN_frame_ext;           // Extended mode
      frame->MsgID = ((uint32_t)p[0]<<21)
                    + (((uint32_t)p[1]&0xe0)<<13)
                    + (((uint32_t)p[1]&0x03)<<16)
                    + ((uint32_t)p[2]<<8)
                    + ((uint32_t)p[3]);
      }
    else
      {
      frame->FIR.B.FF = CAN_frame_std;
      frame->MsgID = ((uint32_t)p[0] << 3) + (p[1] >> 5);  // Standard mode
      }
   
    frame->FIR.B.DLC = p[4] & 0x0f;

    memcpy(&frame->data,p+5,8);
    }

  // handle other interrupts that came in at the same time:
  if (intstat & 0b10100000)
    {
    // Error interrupts:
    //  MERRF 0x80 = message tx/rx error
    //  ERRIF 0x20 = overflow / error state change
    m_error_flags = (intstat & 0b10100000) << 8 | errflag;
  
    if (errflag & 0b10000000) // RXB1 overflow
      { 
      m_errors_rxbuf_overflow++;
      ESP_LOGW(TAG, "CAN Bus 2/3 receive overflow; Frame lost.");
      }
    if (errflag & 0b01000000) // RXB0 overflow.  No data lost in this case (it went into RXB1)
      m_errors_rxbuf_overflow++;
    
    // read error counters:
    uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 2, 2, CMD_READ, 0x1c);
    m_errors_tx = p[0];
    m_errors_rx = p[1];
    
    // clear RX buffer overflow flags:
    m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, 0x2d, 0b11000000, 0x00);
    }
   
  // clear remaining interrupts if any.  Note: Rx ints were cleared when buffers were read; don't clear again here
  if(intstat & 0b11111100) m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, 0x2c, intstat & 0b11111100, 0x00); 

  if(intflag & 0b00000011)   //  did we receive anything?
    return true;
  else 
    return false;
  
  }

void mcp2515::TxCallback()
  {
  }

void mcp2515::SetPowerMode(PowerMode powermode)
  {
  pcp::SetPowerMode(powermode);
  switch (powermode)
    {
    case  On:
      if (m_mode != CAN_MODE_OFF)
        {
        Start(m_mode, m_speed);
        }
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      Stop();
      break;
    default:
      break;
    }
  }
