
/*

    surFami
    SNES emulator
    friol 2k23+

*/

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <GL/GL.h>
#include <tchar.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <chrono>
#include <thread>

#include "resource.h"
#include "imfilebrowser.h"
#include "include/BitmapPlusPlus.hpp"

#include "audioSystem.h"
#include "romLoader.h"
#include "mmu.h"
#include "ppu.h"
#include "apu.h"
#include "cpu5a22.h"
#include "debugger5a22.h"
#include "logger.h"
#include "cpu65816tester.h"
#include "testMMU.h"
#include "debuggerSPC700.h"
#include "spc700tester.h"

struct WGL_WindowData { HDC hDC; };

// Data
static HGLRC            g_hRC;
static WGL_WindowData   g_MainWindow;
static int              g_Width;
static int              g_Height;

// Forward declarations of helper functions
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data);
void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data);
void ResetDeviceWGL();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

logger glbTheLogger;

// Helper functions
bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(data->hDC);
    return true;
}

void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

//
//
//

void prepareVRAMViewerTexture(GLuint& image_texture, int image_width, int image_height, unsigned char* image_data)
{
    // Create a OpenGL texture identifier
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
}

void renderToTexture(GLuint image_texture,int image_width,int image_height,unsigned char* image_data)
{
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        image_width,
        image_height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image_data
    );

    ImGui::Image((void*)(intptr_t)image_texture, ImVec2((float)image_width*2, (float)image_height*2));
}

//
//
//

void displayRomLoadingLogWindow(std::vector<std::string>& romLoadingLog)
{
    ImGui::Begin("ROM loading log window");

    for (auto& log : romLoadingLog)
    {
        ImGui::Text(log.c_str());
    }

    ImGui::End();
}

void displayRegistersWindow(cpu5a22& theCPU,ppu& thePPU,unsigned long int totCPUCycles)
{
    ImGui::Begin("Registers window");
    std::vector<std::string> regInfo = theCPU.getRegistersInfo();
    std::vector<std::string> m7regInfo = thePPU.getM7Matrix();

    for (auto& line : regInfo)
    {
        ImGui::Text(line.c_str());
    }

    std::string cycles = "Tot cycles:" + std::to_string(totCPUCycles);
    ImGui::Text(cycles.c_str());

    for (auto& line : m7regInfo)
    {
        ImGui::Text(line.c_str());
    }

    ImGui::End();
}

void displaySPCRegistersWindow(apu& theAPU)
{
    ImGui::Begin("SPC700 Registers window");
    std::vector<std::string> regInfo = theAPU.getRegisters();

    for (auto& line : regInfo)
    {
        ImGui::Text(line.c_str());
    }

    ImGui::End();
}

void displaySPCDebugWindow(ppu& thePPU, mmu& theMMU, cpu5a22& pCPU, apu& pAPU, debuggerSPC700& pDbgr, bool& rush, unsigned short int& rushAddress,int& emustatus,audioSystem& theAudioSys)
{
    emustatus = emustatus;

    unsigned short int realPC = pAPU.getPC();
    std::vector<std::string> disasmed = pDbgr.disasmOpcodes(realPC,15,&pAPU);

    ImGui::Begin("SPC700 Debuggah");

    //if (emustatus == 0)
    {
        int iidx = 0;
        for (auto& instr : disasmed)
        {
            instr = instr;
            if (iidx == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
                ImGui::Selectable(disasmed[iidx].c_str(), true);
                ImGui::PopStyleColor();
            }
            else ImGui::Selectable(disasmed[iidx].c_str(), false);

            std::string curAddress = disasmed[iidx].substr(0, 4);
            std::string cmd = "Run to address:" + curAddress;

            std::string curElement = "CtxMenu" + std::to_string(iidx);
            if (ImGui::BeginPopupContextItem(curElement.c_str()))
            {
                if (ImGui::Selectable(cmd.c_str()))
                {
                    unsigned short int iAddr;
                    std::stringstream ss;
                    ss << std::hex << curAddress;
                    ss >> iAddr;
                    rushAddress = iAddr;
                    rush = true;
                }
                ImGui::EndPopup();
            }

            iidx += 1;
        }
    }

    // step button
    ImGui::Text(" ");
    if (ImGui::Button("StepOne"))
    {
        int cycs = pAPU.stepOne();
        if (cycs != -1)
        {
            int cpucycs = 0;
            cpucycs+=pCPU.stepOne();
            cpucycs += pCPU.stepOne();
            cpucycs += pCPU.stepOne();
            thePPU.step(cpucycs, theMMU, pCPU);
        }
        pAPU.step(theAudioSys);
    }

    ImGui::End();
}

void displayDebugWindow(cpu5a22& theCPU, debugger5a22& theDebugger5a22, mmu& theMMU,bool& isDebugWindowFocused,bool& rush,int& rushAddress,char* jumpToAppoBuf,unsigned long int& totCPUCycles,int& emustatus,ppu& thePPU)
{
    unsigned int realPC = (theCPU.getPB() << 16) | theCPU.getPC();
    std::vector<disasmInfoRec> disasmed = theDebugger5a22.debugCode(realPC, 20, &theCPU, &theMMU);

    ImGui::Begin("5A22/65C816 Debuggah");

    if (ImGui::IsWindowFocused())
    {
        isDebugWindowFocused = true;
    }
    else isDebugWindowFocused = false;

    //if (emustatus == 0)
    {
        int iidx = 0;
        for (auto& instr : disasmed)
        {
            if (iidx == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
                ImGui::Selectable(instr.disasmed.c_str(), true);
                ImGui::PopStyleColor();
            }
            else ImGui::Selectable(instr.disasmed.c_str(), false);

            std::string curAddress = instr.disasmed.substr(0, 6);
            std::string cmd = "Run to address:" + curAddress;

            std::string curElement = "CtxMenu" + std::to_string(iidx);
            if (ImGui::BeginPopupContextItem(curElement.c_str()))
            {
                if (ImGui::Selectable(cmd.c_str()))
                {
                    rush = true;

                    int iAddr;
                    std::stringstream ss;
                    ss << std::hex << curAddress;
                    ss >> iAddr;
                    rushAddress = iAddr;
                }
                ImGui::EndPopup();
            }

            iidx += 1;
        }
    }

    ImGui::Text(" ");
    ImGui::Text(" ");

    // jumpto
    ImGui::InputText("Step to", jumpToAppoBuf,256); ImGui::SameLine();
    if (ImGui::Button("StepTo!"))
    {
        if (strlen(jumpToAppoBuf) == 0)
        {
            ImGui::OpenPopup("MsgBox");
        }
        else
        {
            rush = true;

            int iAddr;
            std::stringstream ss;
            ss << std::hex << jumpToAppoBuf;
            ss >> iAddr;
            rushAddress = iAddr;
        }

    }

    // step button
    ImGui::Text(" ");
    if (ImGui::Button("StepOne"))
    {
        int cycs= theCPU.stepOne();
        if (cycs != -1)
        {
            totCPUCycles += cycs;
            thePPU.step(cycs,theMMU,theCPU);
        }
    }
    
    ImGui::SameLine();
    // step until UNK
    if (ImGui::Button("Step to UNK"))
    {
        int res = 0;
        while (res != -1)
        {
            res = theCPU.stepOne();
            if (res != -1)
            {
                totCPUCycles += res;
                thePPU.step(res, theMMU, theCPU);
            }
        }
    }

    ImGui::SameLine();
    // step 100
    if (ImGui::Button("Step 100"))
    {
        int res = 0;
        int cycs;
        while (res <100)
        {
            cycs = theCPU.stepOne();
            if (cycs != -1)
            {
                totCPUCycles += cycs;
                thePPU.step(cycs, theMMU, theCPU);
                res += 1;
            }
            else
            {
                res = 100;
            }
        }
    }

    ImGui::SameLine();
    // step 10k
    if (ImGui::Button("Step 100k"))
    {
        int res = 0;
        int cycs;
        while (res < 100000)
        {
            cycs = theCPU.stepOne();
            if (cycs != -1)
            {
                totCPUCycles += cycs;
                thePPU.step(cycs, theMMU, theCPU);
                res += 1;
            }
            else
            {
                res = 10000;
            }
        }
    }

    int pushedColors = 1;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(0.6f, 0.1f, 0.1f)));
    ImGui::SameLine();
    // RUN!
    if (ImGui::Button("Go!"))
    {
        emustatus = 1;
    }
    ImGui::PopStyleColor(pushedColors);

    ImGui::SameLine();
    // Stop
    if (ImGui::Button("Stop"))
    {
        emustatus = 0;
    }

    // error message box
    bool open = true;
    if (ImGui::BeginPopupModal("MsgBox", &open))
    {
        ImGui::Text("Must enter a value");
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::End();
}

void displayLogWindow()
{
    ImGui::Begin("Log window");

    const int nmsgToDisplay = 9;

    if (glbTheLogger.getMessages().size() < nmsgToDisplay)
    {
        for (auto& msg : glbTheLogger.getMessages())
        {
            ImGui::Text(msg.c_str());
        }
    }
    else
    {
        for (int msg = ((int)glbTheLogger.getMessages().size() - nmsgToDisplay);msg < (int)glbTheLogger.getMessages().size();msg++)
        {
            std::string mex = glbTheLogger.getMessages()[msg];
            ImGui::Text(mex.c_str());
        }
    }

    ImGui::End();
}

void displayPaletteWindow(ppu& thePPU)
{
    ImGui::Begin("Palette Window");

    unsigned char palArr[512];
    thePPU.getPalette(palArr);

    ImGui::Text("SNES palette colors");
    int colidx = 0;
    for (int i = 0; i < 64; i++)
    {
        if ((i > 0) && ((i % 16) != 0))
        {
            ImGui::SameLine();
        }
        // palette entry is ?bbbbbgg gggrrrrr
        unsigned int palcol = (((int)(palArr[colidx+1]&0x7f)) << 8) | palArr[colidx];
        int red = palcol & 0x1f; red <<= 3;
        int green = (palcol >> 5) & 0x1f; green <<= 3;
        int blue = (palcol >> 10) & 0x1f; blue <<= 3;

        ImGui::PushID(i);
        ImVec4 colf = ImVec4(((float)(red)) / 255.0f, ((float)((green) & 0xff)) / 255.0f, ((float)((blue) & 0xff)) / 255.0f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, colf);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colf);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colf);
        ImGui::Button(" ");
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        colidx += 2;
    }

    ImGui::End();
}

void displayVRAMViewerWindow(GLuint renderTexture,int image_width,int image_height,unsigned char* parr,ppu& thePPU)
{
    ImGui::Begin("VRAM viewer");

    thePPU.tileViewerRenderTiles();
    renderToTexture(renderTexture,image_width,image_height,parr);

    ImGui::End();
}

void displaySNESScreenWindow(GLuint renderTexture, int image_width, int image_height, unsigned char* parr,bool& isWindowFocused)
{
    ImGui::Begin("SNES TV Output");

    if (ImGui::IsWindowFocused())
    {
        isWindowFocused = true;
    }
    else isWindowFocused = false;

    renderToTexture(renderTexture, image_width, image_height, parr);

    ImGui::End();
}

void displayMemoryWindow(mmu& theMMU,ppu& thePPU,int& baseAddress)
{
    ImGui::Begin("Memory viewer");

    int hSteps = 16;
    int rows = 40;
    int bgmode = thePPU.getCurrentBGMode();
    bgmode++;

    const char* items[] = {"VRAM","Memory","OAM","CGRAM"};
    static const char* current_item = "VRAM";

    if (ImGui::BeginCombo("##combo", current_item))
    {
        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
        {
            bool is_selected = (current_item == items[n]);
            if (ImGui::Selectable(items[n], is_selected))
            {
                current_item = items[n];
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (strcmp(current_item,"VRAM")==0)
    {
        int curAddr = 0x0000;

        for (int r = 0;r < rows;r++)
        {
            std::string sRow;
            std::stringstream strr;
            strr << std::hex << std::setw(6) << std::setfill('0') << curAddr;
            sRow += strr.str() + "  ";

            for (int x = 0;x < hSteps;x += 2)
            {
                unsigned char byteSized0 = thePPU.getVRAMPtr()[curAddr]&0xff;
                unsigned char byteSized1 = thePPU.getVRAMPtr()[curAddr]>>8;

                std::stringstream strrloc;
                strrloc << std::hex << std::setw(2) << std::setfill('0') << (int)byteSized0;
                sRow += strrloc.str() + " ";

                std::stringstream strr2;
                strr2 << std::hex << std::setw(2) << std::setfill('0') << (int)byteSized1;
                sRow += strr2.str() + " ";

                curAddr += 1;
            }

            ImGui::Text(sRow.c_str());
        }
    }
    else if (strcmp(current_item, "CGRAM") == 0)
    {
        int curAddr = 0x0000;

        for (int r = 0;r < 32;r++)
        {
            std::string sRow;
            std::stringstream strr;
            strr << std::hex << std::setw(6) << std::setfill('0') << curAddr;
            sRow += strr.str() + "  ";

            for (int x = 0;x < hSteps;x += 1)
            {
                unsigned char byteSized0 = thePPU.getCGRAMPtr()[curAddr] & 0xff;

                std::stringstream strrloc;
                strrloc << std::hex << std::setw(2) << std::setfill('0') << (int)byteSized0;
                sRow += strrloc.str() + " ";

                curAddr += 1;
            }

            ImGui::Text(sRow.c_str());
        }
    }
    else if (strcmp(current_item, "OAM") == 0)
    {
        int curAddr = 0;
        unsigned char* pOAM = thePPU.getOAMPtr();
        for (int r = 0;r < rows;r++)
        {
            std::string sRow;
            std::stringstream strr;
            strr << std::hex << std::setw(6) << std::setfill('0') << curAddr;
            sRow += strr.str() + "  ";

            for (int x = 0;x < hSteps;x += 1)
            {
                unsigned char byteSized0 = *pOAM;
                //unsigned char byteSized0 = theMMU.read8(curAddr);

                std::stringstream strrloc;
                strrloc << std::hex << std::setw(2) << std::setfill('0') << (int)byteSized0;
                sRow += strrloc.str() + " ";

                curAddr += 1;
                pOAM++;
            }

            ImGui::Text(sRow.c_str());
        }
    }
    else
    {
        int curAddr = baseAddress;

        for (int r = 0;r < rows;r++)
        {
            std::string sRow;
            std::stringstream strr;
            strr << std::hex << std::setw(6) << std::setfill('0') << curAddr;
            sRow += strr.str() + "  ";

            for (int x = 0;x < hSteps;x += 1)
            {
                //unsigned char byteSized0 = *pOAM;
                //pOAM++;
                unsigned char byteSized0 = theMMU.read8(curAddr);
                //unsigned char byteSized0 = thePPU.getVRAMPtr()[curAddr]&0xff;
                //unsigned char byteSized1 = thePPU.getVRAMPtr()[curAddr]>>8;

                std::stringstream strrloc;
                strrloc << std::hex << std::setw(2) << std::setfill('0') << (int)byteSized0;
                sRow += strrloc.str() + " ";

                /*std::stringstream strr2;
                strr2 << std::hex << std::setw(2) << std::setfill('0') << (int)byteSized1;
                sRow += strr2.str() + " ";*/

                curAddr += 1;
            }

            ImGui::Text(sRow.c_str());
        }
    }


    ImGui::End();
}

void displayAppoWindow(cpu5a22& theCPU,ppu& thePPU, mmu& ourMMU, debugger5a22& theDebugger5a22, debuggerSPC700& theDebuggerSPC, ImGui::FileBrowser& fileDialog, std::vector<std::string>& romLoadingLog,int& emustatus,bool& isInitialOpening)
{
    ImGui::Begin("Appo and tests window");
    ImGui::Text("surFami emu: Super Nintendo lives");

    std::string bgModeString = "BG Screen Mode:";
    bgModeString += std::to_string(thePPU.getCurrentBGMode());

    float fps=ImGui::GetIO().Framerate;
    bgModeString += " --- fps: "+std::to_string(fps);

    ImGui::SameLine();
    ImGui::Text(bgModeString.c_str());

    std::vector<debugInfoRec>* pOpcodeList = theDebugger5a22.getOpcodesList();
    std::vector<dbgSPC700info>* pSPCOpcodeList = theDebuggerSPC.getOpcodesList();

    fileDialog.SetTitle("Load SNES ROM");
    fileDialog.SetTypeFilters({ ".smc", ".sfc" });
    fileDialog.SetPwd("d:\\prova\\snes\\");
    //fileDialog.SetPwd("D:\\romz\\nintendo\\snes\\USA\\");
    //fileDialog.SetPwd("D:\\romz\\nintendo\\snes\\szRoms\\");
    
    if (isInitialOpening)
    {
        fileDialog.Open();
        isInitialOpening = false;
    }

    int pushedColors = 1;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(0.6f,0.1f,0.1f)));
    if (ImGui::Button("Load rom!"))
    {
        fileDialog.Open();
    }
    ImGui::SameLine();
    ImGui::PopStyleColor(pushedColors);

    fileDialog.Display();

    if (fileDialog.HasSelected())
    {
        std::string romName = fileDialog.GetSelected().string();
        glbTheLogger.logMsg("Ok, trying to load rom:" + romName);

        bool isHiRom = false;
        int snesStandard = 0;
        romLoader theRomLoader;
        if (theRomLoader.loadRom(romName, ourMMU, romLoadingLog, isHiRom, snesStandard)==0)
        {
            ourMMU.setStandard(snesStandard);
            ourMMU.setHiRom(isHiRom);
            ourMMU.setStandard(snesStandard);

            theCPU.reset();
            ourMMU.setCPU(theCPU);
        }

        emustatus = 1;

        fileDialog.ClearSelected();
    }

    if (ImGui::Button("Start 65816 test"))
    {
        for (auto& testCase : *pOpcodeList)
        {
            if (testCase.validatedTomHarte == false)
            //if (testCase.opcode>=0x61)
            {
                testMMU testMMU;
                cpu5a22 testCPU(&testMMU, true);
                cpu65816tester cpuTester(testMMU, testCPU);

                std::stringstream strr;
                strr << std::hex << std::setw(2) << std::setfill('0') << (int)testCase.opcode;
                std::string curOpcode = strr.str();

                cpuTester.loadTest("D:\\prova\\snes\\ProcessorTests-main\\65816\\v1\\" + curOpcode + ".n.json");
                int retVal = cpuTester.executeTest();

                if (retVal != 0)
                {
                    ImGui::End();
                    return;
                }
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Start SPC700 test"))
    {
        for (auto& testCase : *pSPCOpcodeList)
        {
            if (testCase.isValidatedTomHarte==false)
            {
                testMMU tstMMU;
                apu testAPU;
                spc700tester cpuTester(tstMMU, testAPU);

                std::stringstream strr;
                strr << std::hex << std::setw(2) << std::setfill('0') << (int)testCase.opcode;
                std::string curOpcode = strr.str();

                cpuTester.loadTest("D:\\prova\\snes\\ProcessorTests-main\\spc700\\v1\\" + curOpcode + ".json");
                int retVal = cpuTester.executeTest();

                if (retVal != 0)
                {
                    ImGui::End();
                    return;
                }
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Image"))
    {
        bmp::Bitmap image(thePPU.getPPUResolutionX(),thePPU.getPPUResolutionY());
        unsigned char* pfb = thePPU.getPPUFramebuffer();

        for (bmp::Pixel& pixel : image) 
        {
            bmp::Pixel color{};
            color.r = *pfb; pfb++;
            color.g = *pfb; pfb++;
            color.b = *pfb; pfb++;
            pfb++;
            pixel = color;
        }

        image.save("screenshot.bmp");
    }

    ImGui::SameLine();
    std::string reg4200 = "Reg 0x4200: " + std::to_string(ourMMU.get4200());
    ImGui::Text(reg4200.c_str());

    ImGui::End();
}

//
//
//

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,PSTR lpCmdLine, int nCmdShow)
{
    // stupid visual studio warnings
    hInstance = hInstance;
    hPrevInstance = hPrevInstance;
    lpCmdLine = lpCmdLine;
    nCmdShow = nCmdShow;

    glbTheLogger.logMsg("surFami starting...");

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"surFami", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"surFami", WS_OVERLAPPEDWINDOW, 100, 100, 1520, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize OpenGL
    if (!CreateDeviceWGL(hwnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hwnd, &g_MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsClassic();
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    //

    ppu thePPU;
    apu theAPU;
    if (!theAPU.isBootRomLoaded())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceWGL(hwnd, &g_MainWindow);
        wglDeleteContext(g_hRC);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return 1;
    }
    theAPU.reset();

    mmu theMMU(thePPU,theAPU);
    std::vector<std::string> romLoadingLog;

    //

    debugger5a22 theDebugger5a22;
    debuggerSPC700 theDebuggerSPC700;
    cpu5a22 theCPU(&theMMU,false);

    audioSystem theAudioSys;
    if (!theAudioSys.audioSystemInited)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceWGL(hwnd, &g_MainWindow);
        wglDeleteContext(g_hRC);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return 1;
    }

    //

    GLuint vramRenderTexture;
    prepareVRAMViewerTexture(vramRenderTexture,thePPU.getVRAMViewerXsize(), thePPU.getVRAMViewerYsize(), thePPU.getVRAMViewerBitmap());
    GLuint screenRenderTexture;
    prepareVRAMViewerTexture(screenRenderTexture, thePPU.getPPUResolutionX(), thePPU.getPPUResolutionY(), thePPU.getPPUFramebuffer());

    // 
    // main emu cycle
    //

    bool done = false;
    bool isDebugWindowFocused = false;
    bool isTVWindowFocused = false;
    char jumpToAppoBuf[256];
    jumpToAppoBuf[0] = '\0';
    int baseMemoryAddress = 0x800;
    unsigned long int totCPUCycles = 0;
    int emustatus = -1; // 0 debugging, 1 running, -1 no rom loaded
    ImGui::FileBrowser fileDialog;
    bool isInitialOpening = true;

    std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                done = true;
            }
        }

        if (done)
        {
            break;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_S))
        {
            if (isDebugWindowFocused)
            {
                int cycs= theCPU.stepOne();
                if (cycs != -1)
                {
                    totCPUCycles += cycs;
                    thePPU.step(cycs, theMMU, theCPU);
                }
            }
        }

        if (isTVWindowFocused)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_N))
            {
                theMMU.pressSelectKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_N))
            {
                theMMU.pressSelectKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_M))
            {
                theMMU.pressStartKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_M))
            {
                theMMU.pressStartKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                theMMU.pressRightKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_RightArrow))
            {
                theMMU.pressRightKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                theMMU.pressLeftKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_LeftArrow))
            {
                theMMU.pressLeftKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                theMMU.pressUpKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_UpArrow))
            {
                theMMU.pressUpKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                theMMU.pressDownKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_DownArrow))
            {
                theMMU.pressDownKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Z))
            {
                theMMU.pressAKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_Z))
            {
                theMMU.pressAKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_X))
            {
                theMMU.pressXKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_X))
            {
                theMMU.pressXKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_C))
            {
                theMMU.pressBKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_C))
            {
                theMMU.pressBKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_V))
            {
                theMMU.pressYKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_V))
            {
                theMMU.pressYKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_A))
            {
                theMMU.pressLKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_A))
            {
                theMMU.pressLKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_S))
            {
                theMMU.pressRKey(true);
            }
            else if (ImGui::IsKeyReleased(ImGuiKey_S))
            {
                theMMU.pressRKey(false);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_1)) thePPU.toggleBgActive(0);
            if (ImGui::IsKeyPressed(ImGuiKey_2)) thePPU.toggleBgActive(1);
            if (ImGui::IsKeyPressed(ImGuiKey_3)) thePPU.toggleBgActive(2);
            if (ImGui::IsKeyPressed(ImGuiKey_4)) thePPU.toggleBgActive(3);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        bool rush = false;
        int rushToAddress = 0;

        bool rushSPC = false;
        unsigned short int rushToSPCAddress = 0;

        displayAppoWindow(theCPU,thePPU, theMMU, theDebugger5a22,theDebuggerSPC700, fileDialog,romLoadingLog, emustatus,isInitialOpening);
        displayRomLoadingLogWindow(romLoadingLog);
        displayDebugWindow(theCPU, theDebugger5a22,theMMU,isDebugWindowFocused,rush,rushToAddress,jumpToAppoBuf,totCPUCycles,emustatus,thePPU);
        displaySPCDebugWindow(thePPU, theMMU, theCPU,theAPU, theDebuggerSPC700,rushSPC,rushToSPCAddress,emustatus,theAudioSys);
        displayRegistersWindow(theCPU,thePPU,totCPUCycles);
        displaySPCRegistersWindow(theAPU);
        displayPaletteWindow(thePPU);
        displayLogWindow();
        //displayVRAMViewerWindow(vramRenderTexture, thePPU.getVRAMViewerXsize(), thePPU.getVRAMViewerYsize(), thePPU.getVRAMViewerBitmap(),thePPU);
        displaySNESScreenWindow(screenRenderTexture, thePPU.getPPUResolutionX(), thePPU.getPPUResolutionY(), thePPU.getPPUFramebuffer(),isTVWindowFocused);
        displayMemoryWindow(theMMU,thePPU,baseMemoryAddress);

        if (emustatus == 1)
        {
            int inst = 0;
            while ((inst < 16666)&&(emustatus==1))
            {
                int cycs= theCPU.stepOne();
                if (cycs!=-1)
                {
                    totCPUCycles += cycs;
                    thePPU.step(cycs,theMMU, theCPU);
                }
                else
                {
                    emustatus = 0;
                }
                inst += 1;

                if ((inst % 2) == 0)
                {
                    theAPU.stepOne();
                    theAPU.step(theAudioSys);
                }
            }
        }

        // rush there if needed
        while (rush)
        {
            int curPC = (theCPU.getPB()<<16) | theCPU.getPC();
            if ((curPC&0xffff) == rushToAddress)
            {
                rush = false;
            }
            else
            {
                totCPUCycles+=theCPU.stepOne();
            }
        }

        // rush there for SPC too
        while (rushSPC)
        {
            unsigned short int curPC = theAPU.getPC();
            if (curPC == rushToSPCAddress)
            {
                rushSPC = false;
            }
            else
            {
                totCPUCycles += theCPU.stepOne();
                totCPUCycles += theCPU.stepOne();
                totCPUCycles += theCPU.stepOne();
                theAPU.stepOne();
                theAPU.step(theAudioSys);
            }
        }

        // Rendering
        ImGui::Render();

        glViewport(0, 0, g_Width, g_Height);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        ::SwapBuffers(g_MainWindow.hDC);

        // throttle
        t0 = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> work_time = t0 - t1;
        const float targetMsPerFrame = 1000.0f / 60.0f;

        if (work_time.count() < targetMsPerFrame)
        {
            std::chrono::duration<double, std::milli> delta_ms(targetMsPerFrame - work_time.count());
            auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
        }

        t1 = std::chrono::system_clock::now();
        std::chrono::duration<double, std::milli> sleep_time = t1 - t0;
    }

    // dispose everything

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hwnd, &g_MainWindow);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}
