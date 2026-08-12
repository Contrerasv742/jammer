#pragma once
/*!
 * @file Display.h
 * @brief Display class infrastructure
 * @copyright	Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence     The MIT License (MIT)
 * @maintainer [yangfeng](feng.yang@dfrobot.com)
 * @version  V1.0
 * @maintainer [Victor P.C.](contrerasv742@gmail.com)
 * @date  2026-07-16
 */

#include <Wire.h>
#include <cstdint>
#include <stdio.h>
#include <string.h>


using namespace std;

/*! @brief Device I2C Address */
#define LCD_ADDRESS     (0x7c>>1)

/*! @brief color define */ 
struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

enum class Color : uint8_t { white, red, green, blue };

namespace reg {
    inline constexpr uint8_t mode_1 = 0x00;
    inline constexpr uint8_t mode_2 = 0x01;
    inline constexpr uint8_t output = 0x08;
}
namespace lcd {
    /*!
     *  @brief LCD commands
     */

    namespace cmd
    {
        inline constexpr uint8_t clear_display   = 0x01;
        inline constexpr uint8_t return_home     = 0x02;
        inline constexpr uint8_t entry_mode_set  = 0x04;
        inline constexpr uint8_t display_control = 0x08;
        inline constexpr uint8_t cursor_shift    = 0x10;
        inline constexpr uint8_t function_set    = 0x20;
        inline constexpr uint8_t set_cgram_addr  = 0x40;
        inline constexpr uint8_t set_ddram_addr  = 0x80;
    }

    /* @brief flags entry for display entry mode */
    namespace display::flag
    {
        inline constexpr uint8_t right             = 0x00;
        inline constexpr uint8_t left              = 0x02;
        inline constexpr uint8_t shift_increment   = 0x01;
        inline constexpr uint8_t shift_decrement   = 0x00;
    }

    /* @brief flags for display/blink on/off control */
    namespace display
    {
        inline constexpr uint8_t on         = 0x04;
        inline constexpr uint8_t off        = 0x00;

        inline constexpr uint8_t move       = 0x08;
        inline constexpr uint8_t move_right = 0x04;
        inline constexpr uint8_t move_left  = 0x00;

        inline constexpr uint8_t blink_on    = 0x01;
        inline constexpr uint8_t blink_off   = 0x00;
    }

    /* @brief flags for cursor control */
    namespace cursor
    {
        inline constexpr uint8_t on = 0x02;
        inline constexpr uint8_t off = 0x00;
        inline constexpr uint8_t move = 0x00;
        inline constexpr uint8_t shift    = 0x10;
    }


    /* @brief flags for function set */
    namespace function
    {
        inline constexpr uint8_t bits_8    = 0x10;
        inline constexpr uint8_t bits_4    = 0x00;
        inline constexpr uint8_t lines_2   = 0x08;
        inline constexpr uint8_t lines_1   = 0x00;
        inline constexpr uint8_t dots_5x10 = 0x04;
        inline constexpr uint8_t dots_5x8  = 0x00;
    }
}

class Display {

public:
    /* @brief Constructor */
    Display(uint8_t RGBAddr,uint8_t lcdCols=16,uint8_t lcdRows=2,TwoWire *pWire=&Wire,uint8_t lcdAddr=LCD_ADDRESS);

    /* @brief initialize the LCD and master IIC */ 
    void init();

    /* @brief clear the display and return the cursor to the initial 
     * position (position 0) */
    void clear();

    /* @brief return the cursor to the initial position (0,0) */
    void home();

    /* @brief Turn off the display */
    void noDisplay();

    /**
   * @brief Turn on the display
   */
    void display();

    /* @brief Turn  off the blinking showCursor */
    void stopBlink();

    /* @brief Turn on  the blinking showCursor */
    void blink();

    /* @brief Turn off the underline showCursor */
    void noCursor();

    /* @brief Turn on the underline showCursor */
    void cursor();

    /* @brief scroll left to display */
    void scrollDisplayLeft();

    /* @brief scroll right to display */
    void scrollDisplayRight();

    /* @brief This is for text that flows Left to Right */
    void leftToRight();

    /* @brief This is for text that flows Right to Left */
    void rightToLeft();

    /* @brief This will 'left justify' text from the showCursor */
    void noAutoscroll();

    /* @brief This will 'right justify' text from the showCursor */
    void autoscroll();

    /* @brief Allows us to fill the first 8 CGRAM locations with 
     *        custom characters 
     * @param location substitute character range (0-7)
     * @param charmap  character array the size is 8 bytes
     */
    void customSymbol(uint8_t location, uint8_t charmap[]);

    /** @brief set cursor position
      * @param col columns optional range 0-15
      * @param row rows optional range 0-1，0 is the first row, 1 is 
      *        the second row
     **/
    void setCursor(uint8_t col, uint8_t row);

    /**
   * @brief set RGB
   * @param r  red   range(0-255)
   * @param g  green range(0-255)
   * @param b  blue  range(0-255)
   */
    void setRGB(uint8_t r, uint8_t g, uint8_t b);

    /**
   * @brief set backlight PWM output
   * @param color  backlight color  Preferences：REG_RED\REG_GREEN\REG_BLUE
   * @param pwm  color intensity   range(0-255)
   */
    void setPWM(uint8_t color, uint8_t pwm){setReg(color, pwm); if(RGBAddr_==0x6B){setReg(0x07, pwm);}}      // set pwm

    /**
   * @brief backlight color
   * @param color  backlight color  Preferences： WHITE\RED\GREEN\BLUE
   */
    void setColor(Color color);

    /* @brief close the backlight */
    void closeBacklight(){setRGB(0, 0, 0);}

    /* @brief set the backlight to white */
    void setColorWhite(){setRGB(255, 255, 255);}

    /**
     * @brief write character
     * @param data the written data
     */
    virtual size_t write(uint8_t data);

    /**
     * @brief write string
     * @param str the string to write
     */
    size_t write(const char* str);

    /**
     * @brief write buffer
     * @param buffer the data buffer
     * @param size buffer size
     */
    size_t write(const uint8_t* buffer, size_t size);

    /**
       * @brief send command
       * @param data the sent command
       */
    void command(uint8_t data);

    /**
       * @brief set the backlight
       * @param mode  true indicates the backlight if turned on and set
       *        to white, false indicates the backlight is turned off
       */
    void setBacklight(bool mode);

private:
    /**
   * @brief the initialization function
   * @param row rows optional range 0-1，0 is the first row, 1 is the
   *        second row
   * @param charSize  character size LCD_5x8DOTS\LCD_5x10DOTS
   */
    void begin(uint8_t rows, uint8_t charSize = lcd::function::dots_5x8);

    /**
   * @brief set cursor
   * @param data the data to send
   * @param len length of the data
   */
    void send(uint8_t *data, uint8_t len);

    /**
   * @brief Configure related registers
   * @param addr  The address of the register to be operated on
   * @param data  The data to be written
   */
    void setReg(uint8_t addr, uint8_t data);

    uint8_t showFunction_;
    uint8_t showControl_;
    uint8_t showMode_;
    uint8_t initialized_;
    uint8_t numLines_,currLine_;
    uint8_t lcdAddr_;
    uint8_t RGBAddr_;
    uint8_t cols_;
    uint8_t rows_;
    TwoWire *pWire_;
public:
    uint8_t REG_RED   { 0 };       // pwm2
    uint8_t REG_GREEN { 0 };       // pwm1
    uint8_t REG_BLUE  { 0 };       // pwm0
    uint8_t REG_ONLY  { 0 };       // pwm0
};
