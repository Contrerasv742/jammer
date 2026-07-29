/*!
 * @file Display.cpp
 * @brief Display class infrastructure, the implementation of basic methods
 * @copyright	Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence     The MIT License (MIT)
 * @creator [yangfeng](feng.yang@dfrobot.com)
 * @version  V1.0
 * @maintainer [Victor P.C.](contrerasv742@gmail.com)
 * @date  2026-07-16
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include <stdio.h>
#include <string.h>

#include "Display.h"

namespace {
constexpr Rgb color_of(Color c) {
  switch (c) {
  case Color::white:
    return {255, 255, 255};
  case Color::red:
    return {255, 0, 0};
  case Color::green:
    return {0, 255, 0};
  case Color::blue:
    return {0, 0, 255};
  }
  return {}; // unreachable
}
} // namespace

/*******************************public*******************************/
Display::Display(uint8_t RGBAddr, uint8_t lcdCols, uint8_t lcdRows,
                 TwoWire *pWire, uint8_t lcdAddr) {
  RGBAddr_ = RGBAddr;
  cols_ = lcdCols;
  rows_ = lcdRows;
  lcdAddr_ = lcdAddr;
  pWire_ = pWire;
}

void Display::init() {
  pWire_->begin();
  if (RGBAddr_ == (0x60)) {
    REG_RED = 0x04;
    REG_GREEN = 0x03;
    REG_BLUE = 0x02;
    REG_ONLY = 0x02;
  } else if (RGBAddr_ == (0x60 >> 1)) {
    REG_RED = 0x06;   // pwm2
    REG_GREEN = 0x07; // pwm1
    REG_BLUE = 0x08;  // pwm0
    REG_ONLY = 0x08;
  } else if (RGBAddr_ == (0x6B)) {
    REG_RED = 0x06;   // pwm2
    REG_GREEN = 0x05; // pwm1
    REG_BLUE = 0x04;  // pwm0
    REG_ONLY = 0x04;
  } else if (RGBAddr_ == (0x2D)) {
    REG_RED = 0x01;   // pwm2
    REG_GREEN = 0x02; // pwm1
    REG_BLUE = 0x03;  // pwm0
    REG_ONLY = 0x01;
  }
  showFunction_ =
      lcd::function::bits_4 | lcd::function::lines_1 | lcd::function::dots_5x8;
  begin(rows_);
}

void Display::clear() {
  command(
      lcd::cmd::clear_display);    // clear display, set cursor position to zero
  vTaskDelay(pdMS_TO_TICKS(2000)); // this command takes a long time!
}

void Display::home() {
  command(lcd::cmd::return_home);  // set cursor position to zero
  vTaskDelay(pdMS_TO_TICKS(2000)); // this command takes a long time!
}

void Display::noDisplay() {
  showControl_ &= ~lcd::display::on;
  command(lcd::cmd::display_control | showControl_);
}

void Display::display() {
  showControl_ |= lcd::display::on;
  command(lcd::cmd::display_control | showControl_);
}

void Display::stopBlink() {
  showControl_ &= ~lcd::display::blink_on;
  command(lcd::cmd::display_control | showControl_);
}
void Display::blink() {
  showControl_ |= lcd::display::blink_on;
  command(lcd::cmd::display_control | showControl_);
}

void Display::noCursor() {
  showControl_ &= ~lcd::cursor::on;
  command(lcd::cmd::display_control | showControl_);
}

void Display::cursor() {
  showControl_ |= lcd::cursor::on;
  command(lcd::cmd::display_control | showControl_);
}

void Display::scrollDisplayLeft(void) {
  command(lcd::cmd::cursor_shift | lcd::display::move |
          lcd::display::move_left);
}

void Display::scrollDisplayRight(void) {
  command(lcd::cmd::cursor_shift | lcd::display::move |
          lcd::display::move_right);
}

void Display::leftToRight(void) {
  showMode_ |= lcd::display::flag::left;
  command(lcd::cmd::entry_mode_set | showMode_);
}

void Display::rightToLeft(void) {
  showMode_ &= ~lcd::display::flag::left;
  command(lcd::cmd::entry_mode_set | showMode_);
}

void Display::noAutoscroll(void) {
  showMode_ &= ~lcd::display::flag::shift_increment;
  command(lcd::cmd::entry_mode_set | showMode_);
}

void Display::autoscroll(void) {
  showMode_ |= lcd::display::flag::shift_increment;
  command(lcd::cmd::entry_mode_set | showMode_);
}

void Display::customSymbol(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  command(lcd::cmd::set_cgram_addr | (location << 3));

  uint8_t data[9];
  data[0] = 0x40;
  for (int i = 0; i < 8; i++) {
    data[i + 1] = charmap[i];
  }
  send(data, 9);
}

void Display::setCursor(uint8_t col, uint8_t row) {
  col = (row == 0 ? col | 0x80 : col | 0xc0);
  uint8_t data[3]{0x80, col};

  send(data, 2);
}

void Display::setRGB(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t temp_r, temp_g, temp_b;
  if (RGBAddr_ == 0x60 >> 1) {
    temp_r = (uint16_t)r * 192 / 255;
    temp_g = (uint16_t)g * 192 / 255;
    temp_b = (uint16_t)b * 192 / 255;
    setReg(REG_RED, temp_r);
    setReg(REG_GREEN, temp_g);
    setReg(REG_BLUE, temp_b);
  } else {
    setReg(REG_RED, r);
    setReg(REG_GREEN, g);
    setReg(REG_BLUE, b);
    if (RGBAddr_ == 0x6B) {
      setReg(0x07, 0xFF);
    }
  }
}

void Display::setColor(Color color) {
  const auto [r, g, b] = color_of(color);
  setRGB(r, g, b);
}

inline size_t Display::write(uint8_t value) {
  uint8_t data[3]{0x40, value};
  send(data, 2);
  return 1; // assume success
}

size_t Display::write(const char *str) {
  if (str == nullptr)
    return 0;

  size_t n{0};
  while (*str) {
    write((uint8_t)*str++);
    n++;
  }
  return n;
}

size_t Display::write(const uint8_t *buffer, size_t size) {
  size_t n{0};
  while (size--) {
    write(*buffer++);
    n++;
  }
  return n;
}

inline void Display::command(uint8_t value) {
  uint8_t data[3]{0x80, value};
  send(data, 2);
}

void Display::setBacklight(bool mode) {
  if (mode) {
    setColorWhite(); // turn backlight on
  } else {
    closeBacklight(); // turn backlight off
  }
}

/*******************************private*******************************/
void Display::begin(uint8_t rows, uint8_t charSize) {
  if (rows > 1) {
    showFunction_ |= lcd::function::lines_2;
  }
  numLines_ = rows;
  currLine_ = 0;
  ///< for some 1 line displays you can select a 10 pixel high font
  if ((charSize != 0) && (rows == 1)) {
    showFunction_ |= lcd::function::dots_5x10;
  }

  ///< SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
  ///< according to datasheet, we need at least 40ms after power rises
  ///< above 2.7V before sending commands. Arduino can turn on way befer 4.5V so
  ///< we'll wait 50
  vTaskDelay(pdMS_TO_TICKS(50));

  ///< this is according to the hitachi HD44780 datasheet
  ///< page 45 figure 23

  ///< Send function set command sequence
  command(lcd::cmd::function_set | showFunction_);
  vTaskDelay(pdMS_TO_TICKS(5)); // wait more than 4.1ms

  ///< second try
  command(lcd::cmd::function_set | showFunction_);
  vTaskDelay(pdMS_TO_TICKS(5));

  ///< third go
  command(lcd::cmd::function_set | showFunction_);

  ///< turn the display on with no cursor or blinking default
  showControl_ = lcd::display::on | lcd::cursor::off | lcd::display::blink_off;
  display();

  ///< clear it off
  clear();

  ///< Initialize to default text direction (for romance languages)
  showMode_ = lcd::display::flag::left | lcd::display::flag::shift_decrement;
  ///< set the entry mode
  command(lcd::cmd::entry_mode_set | showMode_);

  if (RGBAddr_ == (0xc0 >> 1)) {
    ///< backlight init
    setReg(reg::mode_1, 0);
    ///< set LEDs controllable by both PWM and GRPPWM registers
    setReg(reg::output, 0xFF);
    ///< set MODE2 values
    ///< 0010 0000 -> 0x20  (DMBLNK to 1, ie blinky mode)
    setReg(reg::mode_2, 0x20);
  } else if (RGBAddr_ == (0x60 >> 1)) {
    setReg(0x01, 0x00);
    setReg(0x02, 0xfF);
    setReg(0x04, 0x15);
  } else if (RGBAddr_ == 0x6B) {
    setReg(0x2F, 0x00);
    setReg(0x00, 0x20);
    setReg(0x01, 0x00);
    setReg(0x02, 0x01);
    setReg(0x03, 4);
  }
  setColorWhite();
}

void Display::send(uint8_t *data, uint8_t len) {
  pWire_->beginTransmission(lcdAddr_); // transmit to device #4
  for (int i{0}; i < len; i++) {
    pWire_->write(data[i]);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  pWire_->endTransmission(); // stop transmitting
}

void Display::setReg(uint8_t addr, uint8_t data) {
  pWire_->beginTransmission(RGBAddr_); // transmit to device #4
  pWire_->write(addr);
  pWire_->write(data);
  pWire_->endTransmission(); // stop transmitting
}
