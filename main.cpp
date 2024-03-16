
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

    ImGui::SameLine();
    // RUN!
    if (ImGui::Button("Go!"))
    {
        emustatus = 1;
    }

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

void displayAppoWindow(ppu& thePPU, mmu& ourMMU, debugger5a22& theDebugger5a22, debuggerSPC700& theDebuggerSPC)
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

    if (ImGui::Button("Patch-it!"))
    {
        ourMMU.write8(0x7e5aaf, 0xea);
        ourMMU.write8(0x7e5ab0, 0xea);
    }
    ImGui::SameLine();

    if (ImGui::Button("Start 65816 test"))
    {
        for (auto& testCase : *pOpcodeList)
        {
            //if (testCase.validatedTomHarte == false)
            if (testCase.opcode>=0x61)
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
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SurFami", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"SurFami", WS_OVERLAPPEDWINDOW, 100, 100, 1580, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize OpenGL
    if (!CreateDeviceWGL(hwnd, &g_MainWindow))
    {
        CleanupDeviceWGL(hwnd, &g_MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g_MainWindow.hDC, g_hRC);

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

    int snesStandard = 0;
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
    bool isHiRom = false;

    romLoader theRomLoader;
    //std::string romName = "d:\\prova\\snes\\HelloWorld.sfc";
    //std::string romName = "d:\\prova\\snes\\BANKWRAM.sfc";
    //std::string romName = "d:\\prova\\snes\\CPUMOV.sfc";
    //std::string romName = "d:\\prova\\snes\\CPUDEC.sfc";
    //std::string romName = "d:\\prova\\snes\\CPUAND.sfc"; // 33
    //std::string romName = "d:\\prova\\snes\\CPUASL.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ADC\\CPUADC.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\BIT\\CPUBIT.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\BRA\\CPUBRA.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\JMP\\CPUJMP.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\CMP\\CPUCMP.sfc"; // c1
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\EOR\\CPUEOR.sfc"; // 52
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\INC\\CPUINC.sfc"; 
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\LDR\\CPULDR.sfc"; // a1
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\LSR\\CPULSR.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\MSC\\CPUMSC.sfc"; // 42
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ORA\\CPUORA.sfc"; // 01
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\PHL\\CPUPHL.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\PSR\\CPUPSR.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\RET\\CPURET.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ROL\\CPUROL.sfc"; // 36
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ROR\\CPUROR.sfc"; // 76
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\SBC\\CPUSBC.sfc"; // f2
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\STR\\CPUSTR.sfc"; // 81
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\TRN\\CPUTRN.sfc"; 
    //std::string romName = "D:\\prova\\snes\\SNES-master\\BANK\\HiROMFastROM\\BANKHiROMFastROM.sfc"; isHiRom = true;
    //std::string romName = "D:\\prova\\snes\\SNES-master\\SPC700\\Twinkle\\Twinkle.sfc";

    //std::string romName = "D:\\prova\\snes\\SNES-master\\Compress\\LZ77\\LZ77WRAMGFX\\LZ77WRAMGFX.sfc"; // mode 5

    //std::string romName = "d:\\prova\\snes\\8x8BG1Map2BPP32x328PAL.sfc";
    //std::string romName = "d:\\prova\\snes\\8x8BGMap4BPP32x328PAL.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\BGMAP\\8x8\\2BPP\\8x8BG2Map2BPP32x328PAL\\8x8BG2Map2BPP32x328PAL.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\BGMAP\\8x8\\8BPP\\TileFlip\\8x8BGMapTileFlip.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\BGMAP\\8x8\\8BPP\\32x32\\8x8BGMap8BPP32x32.sfc"; // bg scrolling
    //std::string romName = "d:\\prova\\snes\\Rings.sfc";
    //std::string romName = "d:\\prova\\snes\\MosaicMode3.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\HDMA\\RedSpaceHDMA\\RedSpaceHDMA.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\HDMA\\HiColor64PerTileRow\\HiColor64PerTileRow.sfc"; 
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\HDMA\\WaveHDMA\\WaveHDMA.sfc"; 
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\HDMA\\RedSpaceIndirectHDMA\\RedSpaceIndirectHDMA.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\Mode7\\RotZoom\\RotZoom.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\Mode7\\StarWars\\StarWars.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\PPU\\Mode7\\Perspective\\Perspective.sfc";
    //std::string romName = "d:\\prova\\snes\\ctrltest_auto.sfc";

    //std::string romName = "d:\\prova\\snes\\Ms. Pac-Man (U).smc";
    //std::string romName = "d:\\prova\\snes\\Super Mario World (USA).sfc";
    //std::string romName = "d:\\prova\\snes\\Super Mario World (J) [!].sfc";
    //std::string romName = "d:\\prova\\snes\\Ninjawarriors (USA).sfc"; // SPC b4
    //std::string romName = "d:\\prova\\snes\\Mario Paint (E) [!].smc"; // SPC 25
    //std::string romName = "d:\\prova\\snes\\Mickey Mania (E).smc"; // stuck
    //std::string romName = "d:\\prova\\snes\\Gradius III (U) [!].smc"; // SPC E9
    //std::string romName = "d:\\prova\\snes\\Parodius (Europe).sfc"; // sprites disappear
    //std::string romName = "d:\\prova\\snes\\Parodius Da! - Shinwa kara Owarai e (Japan).sfc";
    //std::string romName = "d:\\prova\\snes\\Mr. Do! (U).smc"; // very slow
    //std::string romName = "d:\\prova\\snes\\Tetris & Dr Mario (E) [!].smc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\Arkanoid - Doh it Again (E) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Pac Attack (E).smc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\International Superstar Soccer (U) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Kick Off (E).smc"; 
    //std::string romName = "d:\\prova\\snes\\Tetris Attack (E).smc"; snesStandard = 1; // slow
    //std::string romName = "d:\\prova\\snes\\Prince of Persia (E) [!].smc";
    std::string romName = "d:\\prova\\snes\\James Pond's Crazy Sports (E).smc";
    //std::string romName = "d:\\prova\\snes\\Yoshi's Cookie (E).smc"; // SPC 35
    //std::string romName = "d:\\prova\\snes\\Blues Brothers, The (E) [a1].smc"; // SPC 7b
    //std::string romName = "d:\\prova\\snes\\Incredible Crash Dummies, The (U).smc";
    //std::string romName = "d:\\prova\\snes\\Sim City (E) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Gun Force (E).smc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\Space Invaders (U).smc"; // SPC a0
    //std::string romName = "d:\\prova\\snes\\Lawnmower Man, The (E).smc"; // SPC e9
    //std::string romName = "D:\\prova\\snes\\HiRom\\Earthworm Jim (U).smc"; isHiRom = true;
    //std::string romName = "d:\\prova\\snes\\Final Fight (U).smc"; // SPC 57
    //std::string romName = "D:\\prova\\snes\\HiRom\\Super Bomberman (U).smc"; isHiRom = true;
    //std::string romName = "D:\\prova\\snes\\HiRom\\Wolfenstein 3D (U).smc"; isHiRom = true;
    //std::string romName = "d:\\prova\\snes\\F-Zero (U).smc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\Micro Machines (U).smc"; // SPC 17
    //std::string romName = "d:\\prova\\snes\\Robocop 3 (U).smc";
    //std::string romName = "d:\\prova\\snes\\Race Drivin' (U).smc"; // SPC 49
    //std::string romName = "d:\\prova\\snes\\Final Fantasy IV (J).smc"; // SPC 0d
    //std::string romName = "d:\\prova\\snes\\Final Fantasy 4 (tr).sfc";
    //std::string romName = "d:\\prova\\snes\\Addams Family, The (E).smc";
    //std::string romName = "D:\\prova\\snes\\HiRom\\Donkey Kong Country (V1.1) (E).smc"; isHiRom = true; snesStandard = 1; // missing bg on title
    //std::string romName = "D:\\prova\\snes\\HiRom\\Donkey Kong Country (USA) (Rev 2).sfc"; isHiRom = true; // missing bg 
    //std::string romName = "D:\\prova\\snes\\HiRom\\Flashback - The Quest for Identity (U) [!].smc"; isHiRom = true; // SPC 17
    //std::string romName = "d:\\prova\\snes\\The Legend Of Zelda -  A Link To The Past.smc"; // black floor
    //std::string romName = "D:\\romz\\nintendo\\snes\\Earthbound (U).smc"; isHiRom = true; // 
    //std::string romName = "d:\\prova\\snes\\Williams Arcade's Greatest Hits (E) [!].smc"; // a1
    //std::string romName = "d:\\prova\\snes\\Super Back to the Future 2 (J).sfc"; // SPC 17
    //std::string romName = "d:\\prova\\snes\\Unirally (E) [!].smc"; // SPC 5b
    //std::string romName = "d:\\prova\\snes\\Super Turrican (USA).sfc"; // SPC 17
    //std::string romName = "d:\\prova\\snes\\Pinball Dreams (E).smc";
    //std::string romName = "d:\\prova\\snes\\Lemmings (E).sfc"; // hdma/irq problems?
    //std::string romName = "d:\\prova\\snes\\Chessmaster, The (U).smc";

    // HDMA problems
    //std::string romName = "d:\\prova\\snes\\Frogger (U).smc"; snesStandard = 1; // cars are not moving, SPC 49, no music
    //std::string romName = "d:\\prova\\snes\\Puzzle Bobble (E).smc"; // mode4, HDMA not working
    //std::string romName = "d:\\prova\\snes\\Street Fighter II - The World Warrior (U).smc"; // background problems if HDMA enabled, SPC 54
    //std::string romName = "d:\\prova\\snes\\Super Ghouls 'N Ghosts (E).sfc"; // gameplay garbage, HDMA problems, SPC 54

    //std::string romName = "d:\\prova\\snes\\Pocky & Rocky (U).smc";
    //std::string romName = "d:\\prova\\snes\\Super Off Road (E) [!].smc"; // 34, a bg remains uncleared, SPC 5b
    //std::string romName = "d:\\prova\\snes\\Rock N' Roll Racing (U).smc"; // no bg mode3, corrupted graphics, SPC 9d
    //std::string romName = "d:\\prova\\snes\\Spectre (E) [!].smc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\Games\\MonsterFarmJump\\MonsterFarmJump.sfc";
    //std::string romName = "d:\\prova\\snes\\Super Mario All-Stars + Super Mario World (USA).sfc"; // controls don't work
    //std::string romName = "d:\\prova\\snes\\Super Mario All-Stars (U) [!].smc"; // no input
    //std::string romName = "d:\\prova\\snes\\Another World (Europe).sfc"; // SPC 38
    //std::string romName = "d:\\prova\\snes\\Tiny Toons - Wild and Wacky Sports (U).smc"; // stuck after player select 0x2137
    //std::string romName = "d:\\prova\\snes\\Monopoly (V1.1) (U).smc"; // wrong controls, sprite at the start, SPC 77
    //std::string romName = "d:\\prova\\snes\\R-Type 3 (U).smc"; // better but gets stuck, SPC 38
    //std::string romName = "d:\\prova\\snes\\Super Double Dragon (U).smc"; // no input
    //std::string romName = "d:\\prova\\snes\\Animaniacs (U) [!].smc"; // mode 3 missing bg, SPC 35
    //std::string romName = "d:\\prova\\snes\\Home Alone (E) [!].smc"; // resets, SPC 49
    //std::string romName = "d:\\prova\\snes\\SNES Test Program (U).smc"; // mode 5, SPC 31
    //std::string romName = "d:\\prova\\snes\\SHVC-AGING.sfc";
    //std::string romName = "d:\\prova\\snes\\Super Tennis (V1.1) (E) [!].smc"; // stuck, SPC 06
    //std::string romName = "d:\\prova\\snes\\Spanky's Quest (E).smc"; // sub mode 1,stuck
    //std::string romName = "d:\\prova\\snes\\petsciirobotsdemo.sfc"; // stuck
    //std::string romName = "d:\\prova\\snes\\Out of This World (U).smc"; // jumps to nowhere after introduction of VIRQ
    //std::string romName = "d:\\prova\\snes\\Sensible Soccer - International Edition (E).smc"; // blank screen, stuck

    // hiroms
    //std::string romName = "D:\\prova\\snes\\HiRom\\Supercooked! (J) (v1.2).sfc"; isHiRom = true; // resets, SPC 77
    //std::string romName = "D:\\prova\\snes\\HiRom\\Final Fantasy III (USA).sfc"; isHiRom = true; // stuck
    //std::string romName = "D:\\prova\\snes\\HiRom\\Diddy's Kong Quest (E).smc"; isHiRom = true; snesStandard = 1;
    //std::string romName = "D:\\prova\\snes\\HiRom\\Chrono Trigger (U) [!].smc"; isHiRom = true; // 31
    //std::string romName = "D:\\prova\\snes\\HiRom\\Secret of Mana (Europe).sfc"; isHiRom = true; snesStandard = 1; // stuck 4212, SPC 38
    //std::string romName = "D:\\prova\\snes\\HiRom\\Michael Jordan - Chaos in the Windy City (USA).sfc"; isHiRom = true; // missing/bad bg, 13, SPC 35

    // demos
    //std::string romName = "d:\\prova\\snes\\desire_d-zero_snes_pal_revision_2021_oldschool_compo.sfc"; snesStandard = 1; // SPC, reads from CGRAM
    //std::string romName = "d:\\prova\\snes\\elix-smashit-pal.sfc"; snesStandard = 1; // mode6
    //std::string romName = "D:\\prova\\snes\\demos\\elix-nu-pal.sfc"; snesStandard = 1;
    //std::string romName = "D:\\prova\\snes\\demos\\2.68 MHz Demo (PD).sfc"; snesStandard = 1;
    //std::string romName = "D:\\prova\\snes\\demos\\DSR_STNICCC_NOFX_SNES_PAL.sfc"; snesStandard = 1; // jumps to nowhere, no mode7 graphics
    //std::string romName = "D:\\prova\\snes\\demos\\rse-intro.sfc"; snesStandard = 1; // 42, HDMA wrong palette if bAdr is reloaded, SPC 01
    //std::string romName = "d:\\prova\\snes\\demo_mode3.smc"; isHiRom = true;

    theMMU.setStandard(snesStandard);
    theMMU.setHiRom(isHiRom);
    thePPU.setStandard(snesStandard);

    if (theRomLoader.loadRom(romName,theMMU,romLoadingLog,isHiRom) != 0)
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

    debugger5a22 theDebugger5a22;
    debuggerSPC700 theDebuggerSPC700;
    cpu5a22 theCPU(&theMMU,false);
    theCPU.reset();
    theMMU.setCPU(theCPU);

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

    bool done = false;
    bool isDebugWindowFocused = false;
    bool isTVWindowFocused = false;
    char jumpToAppoBuf[256];
    jumpToAppoBuf[0] = '\0';
    int baseMemoryAddress = 0x800;
    unsigned long int totCPUCycles = 0;
    int emustatus = 0; // 0 debugging, 1 running

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
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        bool rush = false;
        int rushToAddress = 0;

        bool rushSPC = false;
        unsigned short int rushToSPCAddress = 0;

        displayAppoWindow(thePPU, theMMU, theDebugger5a22,theDebuggerSPC700);
        displayRomLoadingLogWindow(romLoadingLog);
        displayDebugWindow(theCPU, theDebugger5a22,theMMU,isDebugWindowFocused,rush,rushToAddress,jumpToAppoBuf,totCPUCycles,emustatus,thePPU);
        displaySPCDebugWindow(thePPU, theMMU, theCPU,theAPU, theDebuggerSPC700,rushSPC,rushToSPCAddress,emustatus,theAudioSys);
        displayRegistersWindow(theCPU,thePPU,totCPUCycles);
        displaySPCRegistersWindow(theAPU);
        displayPaletteWindow(thePPU);
        displayLogWindow();
        displayVRAMViewerWindow(vramRenderTexture, thePPU.getVRAMViewerXsize(), thePPU.getVRAMViewerYsize(), thePPU.getVRAMViewerBitmap(),thePPU);
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
                    //theAPU.step(theAudioSys);
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
        /*float fps = ImGui::GetIO().Framerate;
        if (fps > 60.0)
        {
            Sleep(10);
        }*/
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
