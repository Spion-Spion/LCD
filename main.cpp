#include "own_gpio.h"
#include <string>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define LCD_EN_Delay 500
#define LCD_EN_Pulse 500
#define MALLOC_SIZE 128

using namespace std;

/*      LAYOUT OF THE 4BIT DATA ("tab" array) [POx = Pin Output number_of_pin_on_LCD ]
 *
 *      PO4 PO5 PO6 PO7 EN RS
 *      
*/

class LCD_GPIO
{
    public:
    LCD_GPIO();
    ~LCD_GPIO();
    
    int LCD_Init();
    void LCD_Command(unsigned char command); //This function send data to lcd in 4bit mode
    void LCD_String(string str);
    void LCD_xxx(int val);  //The main goal of that function is to change int value to binary form
    void LCD_Clear();
    void LCD_Set_Cursor(int row); //Set line (r=1 line=1, r=2 line=2, the bound of column is 1-16)
    void LCD_Blinking_Cursor(bool state); //Turn on(1) or turn off(0) blinking cursor
    void LCD_Write_Char(char Data);
    
    int LCD_If_error(){return stat;} 
    int LCD_get_terminal_command();
    
    void LCD_get_cpu_temp();
    void LCD_get_memory_usage();

    //private:

    //global variables
    bool tab[8]{0,0,0,0,0,0,0,0};

    //Structures needed to get array from bash
    FILE * stream;
    char * array;

    //these structures are needed to all operations involved with gpio 
    struct gpiochip_info info;
    struct gpioline_info line_info;
    struct gpiohandle_request rq;

    std::string str,str2;
    int fd;
    int stat = 0;
    char read_data[1024];
};

int main(int argc, char *argv[])
{
    const char * temp_command = "vcgencmd measure_temp";
    LCD_GPIO lcd;

    if(lcd.LCD_If_error() > -1)
    {
        if( lcd.LCD_Init() > -1)
        {
            for(int i=0;i<10;i++)
            {

                //send data to first line
                lcd.LCD_Set_Cursor(1);
                lcd.LCD_Clear();
                lcd.LCD_get_cpu_temp();

                //send data to second line
                lcd.LCD_Set_Cursor(2);
                lcd.LCD_get_memory_usage();
                
                usleep(500000); //check info every 1.0 second

              
            }  

            cout << "Closing the program..." << endl;
        }
        

    }
    return 0;
}

void LCD_GPIO::LCD_get_memory_usage()
{
    string total;
    string avail;

    string number_total;
    string number_avail;

    int fd_mem = open("/proc/meminfo", O_NONBLOCK, O_RDONLY);

    if(fd_mem > -1)
    {
        read(fd_mem,read_data,1024);
        lseek(fd_mem,0,SEEK_SET);
    }

    int index = 0;

    total.clear();
    avail.clear();

    for(int i=0;i<512;i++)
    {
        //total
        if(read_data[i] == '\n')
        {
            index++;
            i++;
        }

        if(index == 0)
        {
            total = total + read_data[i];
        }

        //available
        if(index == 2)
        {
            avail = avail + read_data[i];
        }
    }   

    //cout << total << endl;
    //cout << avail << endl;

    for(int i=0;i<total.length();i++)
    {
        if(static_cast<int>(total[i]) >=48 && static_cast<int>(total[i]) <=57)
        {
            number_total = number_total + total[i];
        }
    }

    for(int i=0;i<avail.length();i++)
    {
        if(static_cast<int>(avail[i]) >=48 && static_cast<int>(avail[i]) <=57)
        {
            number_avail = number_avail + avail[i];
        }
    }

    //cout << number_total << endl;
    //cout << number_avail << endl;

    float val_total = stoi(number_total) + 0.00; 
    float val_avail = stoi(number_avail) + 0.00; 

    int pro = (val_avail / val_total) * 100;

    str2.clear();
    str2 = "mem usage: " + to_string(pro) + "%";
    LCD_String(str2);
    close(fd_mem);
}

void LCD_GPIO::LCD_get_cpu_temp()
{
    std::string str;

    str.clear();
    for(int i=0;i<MALLOC_SIZE;i++)
    {
        array[i] = 0;
    }

    stream =popen("vcgencmd measure_temp","r");
    fgets(array,MALLOC_SIZE,stream);
                
    int z = 0;
    while(array[z] != '\n')
    {
        str = str + array[z];
        z++;
    }
    //str = "CPU "+ str;
    pclose(stream);


    LCD_String(str);
}

LCD_GPIO::~LCD_GPIO()
{
    close_file(fd); 
    free(array);
}

LCD_GPIO::LCD_GPIO()
{
    array = (char *)malloc(MALLOC_SIZE);
    for(int i=0;i<MALLOC_SIZE;i++)
    {
        array[i] = 0;
    }
    fd = open_file();
}

void LCD_GPIO::LCD_Blinking_Cursor(bool state)
{
    if(state)
    {
        LCD_Command(0x00);
        LCD_Command(0x0F);
    }
    else
    {
        LCD_Command(0x00);
        LCD_Command(0x0C);
    }
}

void LCD_GPIO::LCD_Set_Cursor(int row)
{
    if(row == 1)
    {
        LCD_Command(0x80 >> 4);
        LCD_Command(0x00);
    }

    if(row == 2)
    {
        LCD_Command(0xC0 >> 4);
        LCD_Command(0x00);
    }
}

void LCD_GPIO::LCD_Write_Char(char Data)
{
    char Low4,High4;
    High4 = Data & 0xF0;
    Low4 = Data & 0x0F;


    LCD_xxx(static_cast<int>(High4>>4));
    LCD_xxx(static_cast<int>(Low4));
}

void LCD_GPIO::LCD_xxx(int val)
{
    // Select Data Register
    tab[5] = 1;

    //Formating data to specyfic index
    for(int i=0;i<4;i++)
    {
        (val & static_cast<int>(pow(2,i)))? tab[i] = 1 : tab[i] = 0;
    }

    // Send The EN Clock Signal
    tab[4] = 1;
    send_8bit(fd,rq,tab);
    usleep(LCD_EN_Delay);

    // Stop Sending The EN Clock Signal
    tab[4] = 0;
    send_8bit(fd,rq,tab);
    usleep(LCD_EN_Delay);

}

void LCD_GPIO::LCD_Clear()
{
    string strx = "                "; // 16x " "
    //string strx = "xxxxxxxxxxxxxxxx"; // 16x " "
    LCD_Set_Cursor(1);
    LCD_String(strx);

    LCD_Set_Cursor(2);
    LCD_String(strx);

    LCD_Set_Cursor(1);
}

void LCD_GPIO::LCD_String(string str)
{
    for(int i=0;i<str.length(); i++)
    {
        LCD_Write_Char(str[i]);
    }
}

void LCD_GPIO::LCD_Command(unsigned char command) //4bit mode
{
    // Select Command Register
    tab[5] = 0;

    for(int i=0;i<4;i++)
    {
        (command & static_cast<int>(pow(2,i)))? tab[i] = 1 : tab[i] = 0;
    }

    // Send The EN Clock Signal
    tab[4] = 1;
    send_8bit(fd,rq,tab);
    usleep(LCD_EN_Pulse);

    // Stop Sending The EN Clock Signal
    tab[4] = 0;
    send_8bit(fd,rq,tab);
    usleep(LCD_EN_Pulse);
}

int LCD_GPIO::LCD_Init()
{
    // The Init. Procedure //1st option
    if(init_chip(fd,info) > -1)
    {
        init_8pins(fd,rq);
        //usleep(50000);

        LCD_Command(0x00);
        usleep(30);
        usleep(LCD_EN_Delay);
        LCD_Command(0x03);
        usleep(5);
        LCD_Command(0x03);
        usleep(150);
        LCD_Command(0x03);
        LCD_Command(0x02);
        LCD_Command(0x02);
        LCD_Command(0x08);
        LCD_Command(0x00);
        LCD_Command(0x0C);
        LCD_Command(0x00);
        LCD_Command(0x06);

        LCD_Command(0x00);
        LCD_Command(0x01);
        stat = 1;
    }
    else
    {
        stat = -1;
    }
    
    return LCD_If_error();
}
