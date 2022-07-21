#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Toggle_Button.H>

#define CMD_PORT 5000
#define IP_ADDR  "192.168.1.4"
#define MAXDATASIZE 1024

int clientfd;
int sockfd;
bool validConnect;

void onConnect(Fl_Widget *, void *)
{
    if (validConnect)
    {
        // TODO need to disconnect
        return;
    }
    
    struct sockaddr_in serv_addr;    
 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(CMD_PORT);

    int res = inet_pton(AF_INET, IP_ADDR, &serv_addr.sin_addr);
    if (res < 1)
    {
        puts("bad address");
        validConnect = false;
        return;
    }
    
    clientfd = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (clientfd < 0)
    {
        close(sockfd);
        puts("connect fail");
        validConnect = false;
        return;
    }
    validConnect = true;
}

void onIPAddr(Fl_Widget *, void *)
{
}

int speedL = 0;
int speedR = 0;

#define STOP 0
#define INCR 250
#define MMAX 3500

char cvtbuffer[1024];

Fl_Value_Slider *slideLeft;
Fl_Value_Slider *slideRight;
Fl_Toggle_Button *slideLock;

const char *cvtToUTF8(char *instr)
{
    int len = strlen(instr);
    for (int i = 0, j=0; i <= len; i++) // NOTE the equal: copy the termination zero!
    {
        cvtbuffer[j++] = instr[i];
        cvtbuffer[j++] = 0;
    }
    return cvtbuffer;
}

void sendMotorAbsolute(int spL, int spR)
{    
    if (!validConnect)
        return; // no connection, nothing to do
        
    speedL = spL;
    speedR = spR;
    
    char buff[1024];
    sprintf(buff, "CMD_MOTOR#%d#%d#%d#%d\n", spL, spL, spR, spR);
    
    //const char *cvted = cvtToUTF8(buff);

    int outlen = strlen(buff);
    send(sockfd, buff, outlen, 0);   
}

void sendMotorDelta(int dLeft, int dRight)
{
    int newL = speedL + dLeft * INCR;
    int newR = speedR + dRight * INCR;
    sendMotorAbsolute(newL, newR);
}

void onStop(Fl_Widget *, void *)
{
    //const char *cmd = u8"CMD_MOTOR#0#0#0#0\n\n";
    //send(sockfd, cmd, strlen(cmd)+1, 0);
    sendMotorAbsolute(STOP, STOP);
}

void onLU(Fl_Widget *, void *)
{
    sendMotorDelta(+1,0);
}

void onLD(Fl_Widget *, void *)
{
    sendMotorDelta(-1,0);
}

void onRU(Fl_Widget *, void *)
{
    sendMotorDelta(0,+1);
}

void onRD(Fl_Widget *, void *)
{
    sendMotorDelta(0,-1);
}

void createBasicTab()
{
    Fl_Group *group = new Fl_Group(10,65,230,295, "Simple");

    int Y = 50;

    auto btnLU = new Fl_Button( 30, Y+50, 25, 25, "+");
    btnLU->callback(onLU);
    auto btnLD = new Fl_Button( 30, Y+125, 25, 25, "-");
    btnLD->callback(onLD);
    auto btnRU = new Fl_Button( 90, Y+50, 25, 25, "+");
    btnRU->callback(onRU);
    auto btnRD = new Fl_Button( 90, Y+125, 25, 25, "-");
    btnRD->callback(onRD);

    auto btnStop = new Fl_Button( 50, Y+85, 50, 25, "Stop");
    btnStop->callback(onStop);

    group->end();
}

void leftChange(Fl_Widget *, void *)
{
    int value = slideLeft->value();
    int spR = speedR;
    if (slideLock->value() != 0)
    {
        spR = value;
        slideRight->value(value);
    }
    sendMotorAbsolute(value, spR);
}

void rightChange(Fl_Widget *, void *)
{
    int value = slideRight->value();
    int spL = speedL;
    if (slideLock->value() != 0)
    {
        spL = value;
        slideLeft->value(value);
    }
    sendMotorAbsolute(spL, value);
}

void onSliderStop(Fl_Widget *, void *)
{
    sendMotorAbsolute(STOP,STOP);
    slideLeft->value(0);
    slideRight->value(0);
}

void createSliderTab()
{
    Fl_Group *group = new Fl_Group(10,65,230,295, "Slider");
    
    Fl_Value_Slider *s1 = new Fl_Value_Slider(25, 75, 40, 250);
    s1->type(FL_VERT_NICE_SLIDER);
    s1->box(FL_BORDER_BOX);
    s1->value(0);
    s1->labelsize(14);
    s1->align(FL_ALIGN_TOP);
    s1->bounds(3500, -3500);
    s1->step(500);
    s1->callback(leftChange);
    slideLeft = s1;
    
    Fl_Value_Slider *s2 = new Fl_Value_Slider(175, 75, 40, 250);
    s2->type(FL_VERT_NICE_SLIDER);
    s2->box(FL_BORDER_BOX);
    s2->value(0);
    s2->labelsize(14);
    s2->align(FL_ALIGN_TOP);
    s2->bounds(3500, -3500);
    s2->step(500);
    s2->callback(rightChange);
    slideRight = s2;
    
    Fl_Button *btnStop = new Fl_Button(90, 200, 50, 25, "Stop");
    btnStop->callback(onSliderStop);
    
    slideLock = new Fl_Toggle_Button(90, 310, 50, 25, "Lock");
    
    group->end();
}

int _activeBtn = 0;
int _activeSpeed = 0;

/*
      9  8  7
      6  5  4
      3  2  1
*/
void BtnUpdate()
{
    double mL, mR;
    
    switch (_activeBtn)
    {
        case 9:
            // hopefully a slower turn left by reversing at half speed
            mL = -0.5;
            mR = +1.0;
            break;
        case 8:
            mL = mR = 1;
            break;
        case 7:
            // hopefully a slower turn right by reversing at half speed
            mL = +1.0;
            mR = -0.5;
            break;
        case 6:
            // turn "in place"
            mL = -1;  
            mR = +1;
            break;
        case 5:
            mL = mR = 0;
            break;
        case 4:
            // turn "in place"
            mL = +1;  
            mR = -1;
            break;
        case 3:
            mL = -0.5;
            mR = -1.0;
            break;
        case 2:
            mL = mR = -1;
            break;
        case 1:
            mL = -1.0;
            mR = -0.5;
            break;
    }
    
    sendMotorAbsolute(_activeSpeed * mL, _activeSpeed * mR);
}

Fl_Toggle_Button *btns[15];

void btnBtnClick(Fl_Widget *w, void *d)
{
    int which = (int)(long)(d);
    
    // toggling a down button up
    if (btns[which]->value() == 0)
    {
        which = 5;
        btns[5]->value(1);
    }
    
    _activeBtn = which;
    
    for (int i = 1; i < 10; i++)
        if (i != _activeBtn)
            btns[i]->value(0);
        
    BtnUpdate();
}

void speedBtnChange(Fl_Widget *w, void *)
{
    int speed = ((Fl_Value_Slider *)w)->value();
    _activeSpeed = speed;
    BtnUpdate();
}

void createButtonTab()
{
    Fl_Group *group = new Fl_Group(10,65,230,295, "Button");
    
    btns[8] = new Fl_Toggle_Button(130, 125, 30, 30, "8");
    btns[8]->callback(btnBtnClick, (void *)8);
    btns[9] = new Fl_Toggle_Button( 90, 125, 30, 30, "9");
    btns[9]->callback(btnBtnClick, (void *)9);
    btns[7] = new Fl_Toggle_Button(170, 125, 30, 30, "7");
    btns[7]->callback(btnBtnClick, (void *)7);
    btns[5] = new Fl_Toggle_Button(130, 170, 30, 30, "@circle");
    btns[5]->callback(btnBtnClick, (void *)7);
    btns[6]  = new Fl_Toggle_Button( 90, 170, 30, 30, "@<");
    btns[6]->callback(btnBtnClick, (void *)6);
    btns[4]  = new Fl_Toggle_Button(170, 170, 30, 30, "@>");
    btns[4]->callback(btnBtnClick, (void *)4);
    btns[2]  = new Fl_Toggle_Button(130, 215, 30, 30, "2");
    btns[2]->callback(btnBtnClick, (void *)2);
    btns[3] = new Fl_Toggle_Button( 90, 215, 30, 30, "3");
    btns[3]->callback(btnBtnClick, (void *)3);
    btns[1] = new Fl_Toggle_Button(170, 215, 30, 30, "1");
    btns[1]->callback(btnBtnClick, (void *)1);

    Fl_Value_Slider *s1 = new Fl_Value_Slider(30, 103, 40, 152);
    s1->type(FL_VERT_NICE_SLIDER);
    s1->box(FL_BORDER_BOX);
    s1->value(0);
    s1->labelsize(14);
    s1->align(FL_ALIGN_TOP);
    s1->bounds(3500,0);
    s1->step(500);
    s1->callback(speedBtnChange);
    
    group->end();
}

int main(int argc, char *argv[])
{
    auto mainwin = new Fl_Window(1000,400,250,400,"pimobile client");

    auto btnConn = new Fl_Button(10, 10, 100, 25, "Connect");
    btnConn->callback(onConnect);

    auto btnIP = new Fl_Button(120, 10, 100, 25, "IP Address");
    btnIP->callback(onIPAddr);
       
    
    Fl_Tabs *tabs = new Fl_Tabs(10, 45, 230, 345);
    tabs->when(FL_WHEN_CHANGED);

    createBasicTab();
    createSliderTab();
    createButtonTab();

    mainwin->resizable(tabs);
    
    mainwin->end();
    mainwin->show(argc,argv);
    return Fl::run();
}

