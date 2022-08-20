#include <stdio.h>
#include <stdlib.h>
#include <mutex>

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
#include <FL/Fl_Input.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Roller.H>

#define HAVE_PTHREAD
#define HAVE_PTHREAD_H
#include "threads.h"

#define CMD_PORT 5000
#define VID_PORT 8000
#define IP_ADDR  "192.168.1.4"
#define MAXDATASIZE 1024

int clientfd;
int sockfd;
int clientfdV;
int sockfdV;
bool validConnect;
bool validVidConn;
char ipAddress[50] = {0};
Fl_Widget *ipAddrView;

Fl_Thread videoThread;
extern void *vidThread(void*);

Fl_Preferences *_prefs;

#define NEW_FRAME 1001 // TODO header
extern unsigned char *imgBuffer; // bytes of jpeg image received from server
std::mutex imageMutex;
Fl_Box *_viewbox;
Fl_Double_Window *_viewwin;

void onConnect(Fl_Widget *w, void *)
{
    if (validConnect)
    {
        pthread_cancel(videoThread);
        close(clientfd);
        close(sockfd);
        close(clientfdV);
        close(sockfdV);
        validConnect = false;
        validVidConn = false;
        w->label("Connect");
        return;
    }

    if (ipAddress[0] == 0)
    {
        fl_alert("No IP Address entered.");
        return;
    }
    
    struct sockaddr_in serv_addr;    
 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(CMD_PORT);

    int res = inet_pton(AF_INET, ipAddress, &serv_addr.sin_addr);
    if (res < 1)
    {
        fl_alert("Invalid IP Address.");
        validConnect = false;
        validVidConn = false;
        return;
    }
    
    clientfd = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (clientfd < 0)
    {
        close(sockfd);
        fl_alert("Connection fail.");
        validConnect = false;
        validVidConn = false;
        return;
    }
    validConnect = true;
    w->label("Disconnect");
    
    // Now connect for video
    {
        sockfdV = socket(AF_INET, SOCK_STREAM, 0);
        serv_addr.sin_port   = htons(VID_PORT);
        int res = inet_pton(AF_INET, ipAddress, &serv_addr.sin_addr);
        clientfdV = connect(sockfdV, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        if (clientfdV < 0)
        {
            close(sockfdV);
            fl_alert("Video connect fail.");
            validVidConn = false;
        }
        else
            validVidConn = true;
    }
    
    // fire video thread
    if (validVidConn)
        fl_create_thread(videoThread, vidThread, NULL);
}

Fl_Window *ipDlg;

void onIPClose(Fl_Widget *, void *)
{
    ipDlg->hide();
}

void getIpAddress()
{
    char *IPAddr;
    _prefs->get("IPAddr", IPAddr, "192.168.1.4");
    strcpy(ipAddress, IPAddr);
    free(IPAddr);
}

void saveIpAddress()
{
    _prefs->set("IPAddr", ipAddress);
}

#include <string.h>

void getWinPos(const char *which, int& x, int& y, int defx, int defy)
{
    char buff[25];
    strcpy(buff, which);
    strcat(buff, "_X");
    _prefs->get(buff, x, defx);
    strcpy(buff, which);
    strcat(buff, "_Y");
    _prefs->get(buff, y, defy);
}

void saveWinPos(const char *which, int x, int y)
{
    char buff[25];
    strcpy(buff, which);
    strcat(buff, "_X");
    _prefs->set(buff, x);
    strcpy(buff, which);
    strcat(buff, "_Y");
    _prefs->set(buff, y);
    _prefs->flush();
}


void onIPAddr(Fl_Widget *, void *)
{
    getIpAddress(); // from prefs
    
    ipDlg = new Fl_Window(200, 200, 200, 100, "IP Address");
    auto ipEdit = new Fl_Input(10, 30, 125, 30, "Enter server IP Address:");
    ipEdit->align(FL_ALIGN_TOP_LEFT);
    auto btnOK = new Fl_Button(10, 70, 100, 25, "Close");
    btnOK->callback(onIPClose);

    if (ipAddress[0])
        ipEdit->value(ipAddress);
    
    ipDlg->end();
    ipDlg->set_modal();
    ipDlg->show();
    while (ipDlg->shown())
        Fl::wait();
    strcpy(ipAddress, ipEdit->value());
    delete ipDlg;
    ipDlg = nullptr;
    
    saveIpAddress();
    ipAddrView->copy_label(ipAddress);    
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

#if 0 // Old simple tab
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
#endif

#if 0 // Old skid steer tab
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
#endif

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

// Active buttons are 1-9
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

void cbOnPress(Fl_Widget *w, void *)
{
    Fl_Check_Button *btn = static_cast<Fl_Check_Button*>(w);
    bool asPress = btn->value() == 1;
    
    // If ON, need to:
    // a. set each direction button's type() value to FL_NORMAL_BUTTON
    // b. set each direction button's when() value to FL_WHEN_CHANGED
    
    // If OFF, need to:
    // a. set each direction button's type() value to FL_TOGGLE_BUTTON
    // b. set each direction button's when() value to FL_WHEN_RELEASE
    
    for (int i=1; i <= 9; i++)
    {
        if (asPress)
        {
            btns[i]->type(FL_NORMAL_BUTTON);
            btns[i]->when(FL_WHEN_CHANGED);
        }
        else
        {
            btns[i]->type(FL_TOGGLE_BUTTON);
            btns[i]->when(FL_WHEN_RELEASE);
        }
    }
}

void createButtonTab()
{
    Fl_Group *group = new Fl_Group(10,65,210,295, "Motor");
    group->box(FL_BORDER_BOX);
    group->color(181);
    
    btns[9] = new Fl_Toggle_Button( 90, 125, 30, 30, "@3<");
    btns[9]->callback(btnBtnClick, (void *)9);
    btns[8] = new Fl_Toggle_Button(130, 125, 30, 30, "@2<");
    btns[8]->callback(btnBtnClick, (void *)8);
    btns[7] = new Fl_Toggle_Button(170, 125, 30, 30, "@1<");
    btns[7]->callback(btnBtnClick, (void *)7);
    btns[6]  = new Fl_Toggle_Button( 90, 170, 30, 30, "@<");
    btns[6]->callback(btnBtnClick, (void *)6);
    btns[5] = new Fl_Toggle_Button(130, 170, 30, 30, "@circle");
    btns[5]->callback(btnBtnClick, (void *)7);
    btns[4]  = new Fl_Toggle_Button(170, 170, 30, 30, "@>");
    btns[4]->callback(btnBtnClick, (void *)4);
    btns[3] = new Fl_Toggle_Button( 90, 215, 30, 30, "@1>");
    btns[3]->callback(btnBtnClick, (void *)3);
    btns[2]  = new Fl_Toggle_Button(130, 215, 30, 30, "@2>");
    btns[2]->callback(btnBtnClick, (void *)2);
    btns[1] = new Fl_Toggle_Button(170, 215, 30, 30, "@3>");
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
    
    Fl_Check_Button *pressToggle = new Fl_Check_Button(30,300,150,25,"On Press");
    pressToggle->callback(cbOnPress);
    
    group->end();
}

void sendServo(char which, int val)
{
    if (!validConnect)
        return; // no connection, nothing to do
            
    char buff[1024];
    sprintf(buff, "CMD_SERVO#%c#%d\n", which, val);
    
    int outlen = strlen(buff);
    send(sockfd, buff, outlen, 0);   
}

void cbServoSlider(Fl_Widget *w, void *)
{
    Fl_Slider *slid = static_cast<Fl_Slider*>(w);
    
    int val = slid->value();
    if (slid->type() == FL_VERT_NICE_SLIDER)
    {
        // TODO move vertical servo
        printf("Vert servo: %d\n", val);
        sendServo('1',val);
    }
    else
    {
        // TODO move horizontal servo
        printf("Horz servo: %d\n", val);
        sendServo('0',val);
    }
}

void createServoTab()
{
    Fl_Group *group = new Fl_Group(230,65,230,295, "Servo");
    group->box(FL_BORDER_BOX);
    group->color(221);
    
    Fl_Value_Slider *vert = new Fl_Value_Slider(245, 120, 30, 150);
    vert->type(FL_VERT_NICE_SLIDER);
    vert->step(10);
    // According to the 'original' client, the range is 180-80.
    vert->bounds(180,80);
    vert->value(90);
    vert->callback(cbServoSlider);
    
    Fl_Value_Slider *horz = new Fl_Value_Slider(245, 85, 200, 30);
    horz->type(FL_HOR_NICE_SLIDER);
    horz->value(90);
    horz->bounds(0,180);
    horz->step(15);
    horz->callback(cbServoSlider);
            
    group->end();
    
    // TODO free-motion sliders?
}

// callback function for handling thread messages
void cbMessage(void *msgV)
{
    long msg = (long)msgV;
    if (msg == NEW_FRAME)
    {
        // release the previous image
        Fl_Image *img = _viewbox->image();
        delete img;
        
        // lock frame buffer & load to Fl_Image
        {
            std::lock_guard<std::mutex> guard(imageMutex);
            img = new Fl_JPEG_Image("a", (const unsigned char *)imgBuffer);
        }

        // show new image in view canvas
        if (img->fail())
            printf("image load failure\n");
        else
        {
            _viewbox->image(img);
            _viewwin->redraw();  // TODO necessary?
            Fl::awake();   // TODO necessary?
        }
        
    }
}

void sendFLTKMsg(long msg)
{
    Fl::awake(cbMessage, (void *)msg);
}

void cbClose(Fl_Widget *w, void *d)
{
    // User has closed the main win; hide it AND the view window to exit
    Fl_Window *win = dynamic_cast<Fl_Window *>(w);
    
    saveWinPos("MAIN", win->x(), win->y());
    saveWinPos("VIEW", _viewwin->x(), _viewwin->y());
    
    _viewwin->hide();
    win->hide();
}

int main(int argc, char *argv[])
{
    Fl::lock();
    
    _prefs = new Fl_Preferences(Fl_Preferences::USER, "fire-eggs", "FreeNoveClient");
    getIpAddress(); // from prefs
    int winx,winy;
    getWinPos("MAIN", winx, winy, 600, 300);

    // TODO size, position from preferences
    auto mainwin = new Fl_Window(winx,winy,470,400,"pimobile client");

    auto btnConn = new Fl_Button(10, 10, 100, 25, "Connect");
    btnConn->callback(onConnect);

    auto btnIP = new Fl_Button(120, 10, 100, 25, "IP Address");
    btnIP->callback(onIPAddr);
    
    auto ipBox = new Fl_Box(230, 10, 125, 25);
    ipBox->box(FL_BORDER_BOX);
    ipBox->copy_label(ipAddress);
    ipBox->align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
    ipAddrView = ipBox;
    
    auto popLEDs = new Fl_Toggle_Button(400, 10, 50, 25, "LEDs");
    popLEDs->deactivate();
       
#if 0    
    Fl_Tabs *tabs = new Fl_Tabs(10, 45, 230, 345);
    tabs->when(FL_WHEN_CHANGED);

    createBasicTab();
    createSliderTab();
#endif    
    createButtonTab();
    createServoTab();

#if 0    
    mainwin->resizable(tabs);
#endif
    mainwin->resizable(mainwin);
    mainwin->end();
    mainwin->callback(cbClose);

#if true    // temporary
    getWinPos("VIEW", winx, winy, 700, 350);
    _viewwin = new Fl_Double_Window(winx, winy, 800, 600, "pimobile view");
    _viewbox = new Fl_Box(0,0,800, 600);
    _viewwin->end();
    _viewwin->show();
#endif    
    mainwin->show(argc,argv);
    
    Fl::awake(cbMessage, nullptr);
    
    return Fl::run();
}
