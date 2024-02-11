
/*

    surFami
    snes emulator
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

#include "romLoader.h"
#include "mmu.h"
#include "ppu.h"
#include "apu.h"
#include "cpu5a22.h"
#include "debugger5a22.h"
#include "logger.h"
#include "cpu65816tester.h"
#include "testMMU.h"

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

    int iidx = 0;
    for (auto& instr : disasmed)
    {
        if (iidx==0)
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
    int rows = 20;
    int bgmode = thePPU.getCurrentBGMode();
    bgmode++;

    const char* items[] = {"VRAM","Memory","OAM"};
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
        int curAddr = 0x2000;

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
    else if (strcmp(current_item, "OAM") == 0)
    {
        int curAddr = 0;
        for (int r = 0;r < rows;r++)
        {
            std::string sRow;
            std::stringstream strr;
            strr << std::hex << std::setw(6) << std::setfill('0') << curAddr;
            sRow += strr.str() + "  ";

            for (int x = 0;x < hSteps;x += 1)
            {
                unsigned char* pOAM = thePPU.getOAMPtr();
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

void displayAppoWindow(ppu& thePPU, mmu& ourMMU, debugger5a22& theDebugger5a22)
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

    if (ImGui::Button("Start test"))
    {
        for (auto& testCase : *pOpcodeList)
        {
            if (testCase.validatedTomHarte == false)
            //if (testCase.opcode>=0x00)
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
    mmu theMMU(thePPU,theAPU);
    std::vector<std::string> romLoadingLog;
    bool isHiRom = false;

    romLoader theRomLoader;
    //std::string romName = "d:\\prova\\snes\\HelloWorld.sfc";
    //std::string romName = "d:\\prova\\snes\\BANKWRAM.sfc";
    //std::string romName = "d:\\prova\\snes\\CPUMOV.sfc";
    //std::string romName = "d:\\prova\\snes\\CPUDEC.sfc";
    //std::string romName = "d:\\prova\\snes\\CPUAND.sfc"; // 21 unk
    //std::string romName = "d:\\prova\\snes\\CPUASL.sfc"; // 16 unk
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ADC\\CPUADC.sfc"; // 61 unk
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\BIT\\CPUBIT.sfc"; // 3c unk
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\BRA\\CPUBRA.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\JMP\\CPUJMP.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\CMP\\CPUCMP.sfc"; // d5
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\EOR\\CPUEOR.sfc"; // 4f
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\INC\\CPUINC.sfc"; 
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\LDR\\CPULDR.sfc"; // a1
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\LSR\\CPULSR.sfc"; // 56
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\MSC\\CPUMSC.sfc"; // 42
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ORA\\CPUORA.sfc"; // 12
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\PHL\\CPUPHL.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\PSR\\CPUPSR.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\RET\\CPURET.sfc";
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ROL\\CPUROL.sfc"; // 36
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\ROR\\CPUROR.sfc"; // 76
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\SBC\\CPUSBC.sfc"; // f2
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\STR\\CPUSTR.sfc"; // 81
    //std::string romName = "D:\\prova\\snes\\SNES-master\\CPUTest\\CPU\\TRN\\CPUTRN.sfc"; 

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
    //std::string romName = "d:\\prova\\snes\\Super Mario World (J) [!].sfc"; // 5f
    //std::string romName = "d:\\prova\\snes\\Mario Paint (E) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Mickey Mania (E).smc";
    //std::string romName = "d:\\prova\\snes\\Gradius III (U) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Parodius (Europe).sfc";
    //std::string romName = "d:\\prova\\snes\\Parodius Da! - Shinwa kara Owarai e (Japan).sfc";
    //std::string romName = "d:\\prova\\snes\\Mr. Do! (U).smc";
    //std::string romName = "d:\\prova\\snes\\Tetris & Dr Mario (E) [!].smc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\Arkanoid - Doh it Again (E) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Pac Attack (E).smc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\International Superstar Soccer (U) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Kick Off (E).smc";
    //std::string romName = "d:\\prova\\snes\\Tetris Attack (E).smc"; snesStandard = 1;
    std::string romName = "d:\\prova\\snes\\Prince of Persia (E) [!].smc";
    //std::string romName = "d:\\prova\\snes\\James Pond's Crazy Sports (E).smc";
    //std::string romName = "d:\\prova\\snes\\Yoshi's Cookie (E).smc";
    //std::string romName = "d:\\prova\\snes\\Blues Brothers, The (E) [a1].smc";
    //std::string romName = "d:\\prova\\snes\\Incredible Crash Dummies, The (U).smc";
    //std::string romName = "d:\\prova\\snes\\Sim City (E) [!].smc";

    // suspect HDMA problems
    //std::string romName = "d:\\prova\\snes\\Frogger (U).smc"; snesStandard = 1; // corrupted frog, cars are not moving
    //std::string romName = "d:\\prova\\snes\\F-Zero (U).smc"; snesStandard = 1; // mode 7 corrupted perspective, sprites bug (maybe HDMA)
    //std::string romName = "d:\\prova\\snes\\Puzzle Bobble (E).smc"; // mode4, HDMA not working
    //std::string romName = "d:\\prova\\snes\\Gun Force (E).smc"; snesStandard = 1; // black screen with new HDMA?

    //std::string romName = "d:\\prova\\snes\\Tiny Toons - Wild and Wacky Sports (U).smc"; // stuck after player select
    //std::string romName = "d:\\prova\\snes\\Monopoly (V1.1) (U).smc"; // gah
    //std::string romName = "d:\\prova\\snes\\Space Invaders (U).smc"; // screen blackens
    //std::string romName = "d:\\prova\\snes\\Final Fight (U).smc"; // missing bgs, screen blacks out in gameplay
    //std::string romName = "d:\\prova\\snes\\Lemmings (E).sfc"; // bg corrupted in mode 1, then crashes
    //std::string romName = "d:\\prova\\snes\\Super Mario All-Stars + Super Mario World (USA).sfc"; // stuck somewhere before first screen
    //std::string romName = "d:\\prova\\snes\\Super Mario All-Stars (U) [!].smc"; // stuck
    //std::string romName = "d:\\prova\\snes\\Pinball Dreams (E).smc"; // stuck, no ball
    //std::string romName = "d:\\prova\\snes\\R-Type 3 (U).smc"; // no graphics
    //std::string romName = "d:\\prova\\snes\\Street Fighter II - The World Warrior (U).smc"; snesStandard = 1; // blank screen
    //std::string romName = "d:\\prova\\snes\\Micro Machines (U).smc"; // stuck
    //std::string romName = "d:\\prova\\snes\\Super Double Dragon (U).smc"; // no input
    //std::string romName = "d:\\prova\\snes\\Robocop 3 (U).smc"; // strange robocop sprite
    //std::string romName = "d:\\prova\\snes\\Home Alone (E) [!].smc"; // resets

    //std::string romName = "d:\\prova\\snes\\SNES Test Program (U).smc"; // mode 5
    //std::string romName = "d:\\prova\\snes\\Chessmaster, The (U).smc"; // dma mode 4?
    //std::string romName = "d:\\prova\\snes\\Final Fantasy IV (J).smc"; // stuck
    //std::string romName = "d:\\prova\\snes\\Race Drivin' (U).smc"; // wrong tiles
    //std::string romName = "d:\\prova\\snes\\Super Tennis (V1.1) (E) [!].smc"; // stuck 
    //std::string romName = "d:\\prova\\snes\\Super Off Road (E) [!].smc"; // 34, a bg remains uncleared
    //std::string romName = "d:\\prova\\snes\\Sensible Soccer - International Edition (E).smc"; // blank screen
    //std::string romName = "d:\\prova\\snes\\The Legend Of Zelda -  A Link To The Past.smc"; // bad bg, strange sprites
    //std::string romName = "d:\\prova\\snes\\Spanky's Quest (E).smc"; // stuck
    //std::string romName = "d:\\prova\\snes\\Spectre (E) [!].smc";
    //std::string romName = "d:\\prova\\snes\\Lawnmower Man, The (E).smc"; 
    //std::string romName = "d:\\prova\\snes\\Williams Arcade's Greatest Hits (E) [!].smc"; // a1
    //std::string romName = "d:\\prova\\snes\\Unirally (E) [!].smc"; // crash, SRAM test
    //std::string romName = "d:\\prova\\snes\\petsciirobotsdemo.sfc"; snesStandard = 1; // stuck, missing part of screen
    //std::string romName = "d:\\prova\\snes\\Out of This World (U).smc"; // stuck LDA 0x0800
    //std::string romName = "D:\\prova\\snes\\SNES-master\\Games\\MonsterFarmJump\\MonsterFarmJump.sfc";

    // hiroms
    //std::string romName = "D:\\prova\\snes\\HiRom\\Flashback - The Quest for Identity (U) [!].smc"; isHiRom = true;
    //std::string romName = "D:\\romz\\nintendo\\snes\\Earthbound (U).smc"; isHiRom = true;
    //std::string romName = "D:\\prova\\snes\\HiRom\\Donkey Kong Country (V1.1) (E).smc"; isHiRom = true; snesStandard = 1; 
    //std::string romName = "D:\\prova\\snes\\HiRom\\Final Fantasy III (USA).sfc"; isHiRom = true; // corrupted bg mode 1
    //std::string romName = "D:\\prova\\snes\\HiRom\\Earthworm Jim (U).smc"; isHiRom = true;
    //std::string romName = "D:\\prova\\snes\\HiRom\\Super Bomberman (U).smc"; isHiRom = true; // black screen on mode 1
    //std::string romName = "D:\\prova\\snes\\HiRom\\Diddy's Kong Quest (E).smc"; isHiRom = true; snesStandard = 1;

    // demos
    //std::string romName = "d:\\prova\\snes\\desire_d-zero_snes_pal_revision_2021_oldschool_compo.sfc"; snesStandard = 1;
    //std::string romName = "d:\\prova\\snes\\elix-smashit-pal.sfc"; snesStandard = 1; // mode6
    //std::string romName = "D:\\prova\\snes\\demos\\elix-nu-pal.sfc"; snesStandard = 1;
    //std::string romName = "D:\\prova\\snes\\demos\\2.68 MHz Demo (PD).sfc"; snesStandard = 1; // stuck on wai
    //std::string romName = "D:\\prova\\snes\\demos\\DSR_STNICCC_NOFX_SNES_PAL.sfc"; // no mode7 graphics
    //std::string romName = "D:\\prova\\snes\\demos\\rse-intro.sfc"; snesStandard = 1; // 42, HDMA problems
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

    // unfortunately, no SPC cpu is emulated at this time
    if (romName == "d:\\prova\\snes\\Frogger (U).smc")
    {
        theMMU.write8(0x8ff1fa, 0xea);
        theMMU.write8(0x8ff1fb, 0xea);
        theMMU.write8(0x8ff141, 0xea);
        theMMU.write8(0x8ff142, 0xea);
        theMMU.write8(0x8ff108, 0xea);
        theMMU.write8(0x8ff109, 0xea);
        theMMU.write8(0x8ff118, 0xea);
        theMMU.write8(0x8ff119, 0xea);
        theMMU.write8(0x8ff2ce, 0xea);
        theMMU.write8(0x8ff2cf, 0xea);
        theMMU.write8(0x8ff353, 0xea);
        theMMU.write8(0x8ff354, 0xea);
        theMMU.write8(0x8ff378, 0xea);
        theMMU.write8(0x8ff379, 0xea);
        theMMU.write8(0x8ff38e, 0xea);
        theMMU.write8(0x8ff38f, 0xea);
        theMMU.write8(0x8ff201, 0xea);
        theMMU.write8(0x8ff202, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Unirally (E) [!].smc")
    {
        theMMU.write8(0x82810d, 0xea);
        theMMU.write8(0x82810e, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Sim City (E) [!].smc")
    {
        theMMU.write8(0x059370, 0xea);
        theMMU.write8(0x059371, 0xea);
        theMMU.write8(0x05937b, 0xea);
        theMMU.write8(0x05937c, 0xea);
        theMMU.write8(0x059380, 0xea);
        theMMU.write8(0x059381, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Mr. Do! (U).smc")
    {
        theMMU.write8(0x684c2, 0xea);
        theMMU.write8(0x684c3, 0xea);
        theMMU.write8(0x684d7, 0xea);
        theMMU.write8(0x684d8, 0xea);
        theMMU.write8(0x684eb, 0xea);
        theMMU.write8(0x684ec, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Yoshi's Cookie (E).smc")
    {
        theMMU.write8(0x0d8162, 0xea);
        theMMU.write8(0x0d8163, 0xea);
        theMMU.write8(0x0d8179, 0xea);
        theMMU.write8(0x0d817a, 0xea);
        theMMU.write8(0x0d844c, 0xea);
        theMMU.write8(0x0d844d, 0xea);
        theMMU.write8(0x0d8459, 0xea);
        theMMU.write8(0x0d845a, 0xea);
        theMMU.write8(0x0d846c, 0xea);
        theMMU.write8(0x0d846d, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Tetris Attack (E).smc")
    {
        theMMU.write8(0x80941a, 0xea);
        theMMU.write8(0x80941b, 0xea);
        theMMU.write8(0x80947c, 0xea);
        theMMU.write8(0x80947d, 0xea);
        theMMU.write8(0x809481, 0xea);
        theMMU.write8(0x809482, 0xea);
        theMMU.write8(0x8094c7, 0xea);
        theMMU.write8(0x8094c8, 0xea);
        theMMU.write8(0x8094cc, 0xea);
        theMMU.write8(0x8094cd, 0xea);
        theMMU.write8(0x8094d8, 0xea);
        theMMU.write8(0x8094d9, 0xea);
        theMMU.write8(0x8094dd, 0xea);
        theMMU.write8(0x8094de, 0xea);
        theMMU.write8(0x809556, 0xea);
        theMMU.write8(0x809557, 0xea);
        theMMU.write8(0x80955b, 0xea);
        theMMU.write8(0x80955c, 0xea);
        theMMU.write8(0x809571, 0xea);
        theMMU.write8(0x809572, 0xea);
        theMMU.write8(0x809576, 0xea);
        theMMU.write8(0x809577, 0xea);
        theMMU.write8(0x809583, 0xea);
        theMMU.write8(0x809584, 0xea);
        theMMU.write8(0x809588, 0xea);
        theMMU.write8(0x809589, 0xea);
        
    }
    else if (romName == "d:\\prova\\snes\\Blues Brothers, The (E) [a1].smc")
    {
        theMMU.write8(0x80842e, 0xea);
        theMMU.write8(0x80842f, 0xea);
        theMMU.write8(0x80845c, 0xea);
        theMMU.write8(0x80845d, 0xea);
        theMMU.write8(0x808485, 0xea);
        theMMU.write8(0x808486, 0xea);
        theMMU.write8(0x8084bc, 0xea);
        theMMU.write8(0x8084bd, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Tetris & Dr Mario (E) [!].smc")
    {
        theMMU.write8(0x80bb89, 0xea);
        theMMU.write8(0x80bb8a, 0xea);
        theMMU.write8(0x80bb9d, 0xea);
        theMMU.write8(0x80bb9e, 0xea);
        theMMU.write8(0x80bb45, 0xea);
        theMMU.write8(0x80bb46, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\desire_d-zero_snes_pal_revision_2021_oldschool_compo.sfc")
    {
        theMMU.write8(0xa239, 0xea);
        theMMU.write8(0xa23a, 0xea);
        theMMU.write8(0xa253, 0xea);
        theMMU.write8(0xa254, 0xea);
        theMMU.write8(0xa2a1, 0xea);
        theMMU.write8(0xa2a2, 0xea);
        theMMU.write8(0xa278, 0xea);
        theMMU.write8(0xa279, 0xea);
        theMMU.write8(0xa2c4, 0xea);
        theMMU.write8(0xa2c5, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Prince of Persia (E) [!].smc")
    {
        theMMU.write8(0x2985a, 0xea);
        theMMU.write8(0x2985b, 0xea);
        theMMU.write8(0x298a7, 0xea);
        theMMU.write8(0x298a8, 0xea);
        theMMU.write8(0x298b2, 0xea);
        theMMU.write8(0x298b3, 0xea);
        theMMU.write8(0x298c2, 0xea);
        theMMU.write8(0x298c3, 0xea);
        theMMU.write8(0x298d3, 0xea);
        theMMU.write8(0x298d4, 0xea);
        theMMU.write8(0x298df, 0xea);
        theMMU.write8(0x298e0, 0xea);
        theMMU.write8(0x298ed, 0xea);
        theMMU.write8(0x298ee, 0xea);
        theMMU.write8(0x2990b, 0xea);
        theMMU.write8(0x2990c, 0xea);
        theMMU.write8(0x29921, 0xea);
        theMMU.write8(0x29922, 0xea);
        theMMU.write8(0x2992f, 0xea);
        theMMU.write8(0x29930, 0xea);
        theMMU.write8(0x29962, 0xea);
        theMMU.write8(0x29963, 0xea);
        theMMU.write8(0x29999, 0xea);
        theMMU.write8(0x2999a, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\The Legend Of Zelda -  A Link To The Past.smc")
    {
        theMMU.write8(0x88ef, 0xea);
        theMMU.write8(0x88f0, 0xea);
        theMMU.write8(0x88b6, 0xea);
        theMMU.write8(0x88b7, 0xea);
        theMMU.write8(0x88c6, 0xea);
        theMMU.write8(0x88c7, 0xea);
        
        theMMU.write8(0xf3b2, 0xea);
        theMMU.write8(0xf3b3, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Gun Force (E).smc")
    {
        theMMU.write8(0x8116, 0xea);
        theMMU.write8(0x8117, 0xea);
        theMMU.write8(0x80dd, 0xea);
        theMMU.write8(0x80de, 0xea);
        theMMU.write8(0x80ed, 0xea);
        theMMU.write8(0x80ee, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Arkanoid - Doh it Again (E) [!].smc")
    {
        theMMU.write8(0x84807a, 0xea);
        theMMU.write8(0x84807b, 0xea);
        theMMU.write8(0x848096, 0xea);
        theMMU.write8(0x848097, 0xea);
        theMMU.write8(0x809791, 0xea);
        theMMU.write8(0x809792, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Race Drivin' (U).smc")
    {
        theMMU.write8(0xfe3b6, 0xea);
        theMMU.write8(0xfe3b7, 0xea);
        theMMU.write8(0xfe37d, 0xea);
        theMMU.write8(0xfe37e, 0xea);
        theMMU.write8(0xfe38d, 0xea);
        theMMU.write8(0xfe38e, 0xea);
        theMMU.write8(0xfe3d6, 0xea);
        theMMU.write8(0xfe3d7, 0xea);
        theMMU.write8(0xfe3e4, 0xea);
        theMMU.write8(0xfe3e5, 0xea);
        theMMU.write8(0xfe4ec, 0xea);
        theMMU.write8(0xfe4ed, 0xea);
        theMMU.write8(0xfe512, 0xea);
        theMMU.write8(0xfe513, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Super Tennis (V1.1) (E) [!].smc")
    {
        theMMU.write8(0x7d902, 0xea);
        theMMU.write8(0x7d903, 0xea);
        theMMU.write8(0x7d8c9, 0xea);
        theMMU.write8(0x7d8ca, 0xea);
        theMMU.write8(0x7d8d9, 0xea);
        theMMU.write8(0x7d8da, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Kick Off (E).smc")
    {
        theMMU.write8(0x86a2, 0xea);
        theMMU.write8(0x86a3, 0xea);
        theMMU.write8(0x8669, 0xea);
        theMMU.write8(0x866a, 0xea);
        theMMU.write8(0x8679, 0xea);
        theMMU.write8(0x867a, 0xea);
        theMMU.write8(0xd8008, 0xea);
        theMMU.write8(0xd8009, 0xea);
        theMMU.write8(0xd85f8, 0xea);
        theMMU.write8(0xd85f9, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Home Alone (E) [!].smc")
    {
        theMMU.write8(0x867c, 0xea);
        theMMU.write8(0x867d, 0xea);
        theMMU.write8(0x8643, 0xea);
        theMMU.write8(0x8644, 0xea);
        theMMU.write8(0x8653, 0xea);
        theMMU.write8(0x8654, 0xea);
        theMMU.write8(0x876c, 0xea);
        theMMU.write8(0x876d, 0xea);
        theMMU.write8(0x879b, 0xea);
        theMMU.write8(0x879c, 0xea);
        theMMU.write8(0x87b2, 0xea);
        theMMU.write8(0x87b3, 0xea);
        theMMU.write8(0x87d7, 0xea);
        theMMU.write8(0x87d8, 0xea);
        theMMU.write8(0x8838, 0xea);
        theMMU.write8(0x8839, 0xea);
        theMMU.write8(0x87ec, 0xea);
        theMMU.write8(0x87ed, 0xea);
        theMMU.write8(0x8803, 0xea);
        theMMU.write8(0x8804, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\elix-smashit-pal.sfc")
    {
        theMMU.write8(0x8527, 0xea);
        theMMU.write8(0x8528, 0xea);
        theMMU.write8(0x8542, 0xea);
        theMMU.write8(0x8543, 0xea);
        theMMU.write8(0x8561, 0xea);
        theMMU.write8(0x8562, 0xea);
        theMMU.write8(0x8581, 0xea);
        theMMU.write8(0x8582, 0xea);
        theMMU.write8(0x84d7, 0xea);
        theMMU.write8(0x84d8, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Super Off Road (E) [!].smc")
    {
        theMMU.write8(0xe096, 0xea);
        theMMU.write8(0xe097, 0xea);
        theMMU.write8(0xe060, 0xea);
        theMMU.write8(0xe061, 0xea);
        theMMU.write8(0xe070, 0xea);
        theMMU.write8(0xe071, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Parodius (Europe).sfc")
    {
        theMMU.write8(0xa80d, 0xea);
        theMMU.write8(0xa80e, 0xea);
        theMMU.write8(0xa7d1, 0xea);
        theMMU.write8(0xa7d2, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\SNES Test Program (U).smc")
    {
        theMMU.write8(0xd894, 0xea);
        theMMU.write8(0xd895, 0xea);
        theMMU.write8(0xd85b, 0xea);
        theMMU.write8(0xd85c, 0xea);
        theMMU.write8(0xd86b, 0xea);
        theMMU.write8(0xd86c, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Frogger (U).smc")
    {
        theMMU.write8(0x8ff141, 0xea);
        theMMU.write8(0x8ff142, 0xea);
        theMMU.write8(0x8ff108, 0xea);
        theMMU.write8(0x8ff109, 0xea);
        theMMU.write8(0x8ff118, 0xea);
        theMMU.write8(0x8ff119, 0xea);
        theMMU.write8(0x8ff2ce, 0xea);
        theMMU.write8(0x8ff2cf, 0xea);
        theMMU.write8(0x8ff353, 0xea);
        theMMU.write8(0x8ff354, 0xea);
        theMMU.write8(0x8ff334, 0xea);
        theMMU.write8(0x8ff335, 0xea);
        theMMU.write8(0x8ff378, 0xea);
        theMMU.write8(0x8ff379, 0xea);
        theMMU.write8(0x8ff38e, 0xea);
        theMMU.write8(0x8ff38f, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Chessmaster, The (U).smc")
    {
        theMMU.write8(0xaa50, 0xea);
        theMMU.write8(0xaa51, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Ms. Pac-Man (U).smc")
    {
        theMMU.write8(0x8387a2, 0xea);
        theMMU.write8(0x8387a3, 0xea);
        theMMU.write8(0x838763, 0xea);
        theMMU.write8(0x838764, 0xea);
        theMMU.write8(0x838773, 0xea);
        theMMU.write8(0x838774, 0xea);
        theMMU.write8(0x8387aa, 0xea);
        theMMU.write8(0x8387ab, 0xea);
        theMMU.write8(0x838c60, 0xea);
        theMMU.write8(0x838c61, 0xea);
        theMMU.write8(0x8389d7, 0xea);
        theMMU.write8(0x8389d8, 0xea);
        theMMU.write8(0x838a03, 0xea);
        theMMU.write8(0x838a04, 0xea);
        theMMU.write8(0x838a28, 0xea);
        theMMU.write8(0x838a29, 0xea);
        theMMU.write8(0x838a3b, 0xea);
        theMMU.write8(0x838a3c, 0xea);
        theMMU.write8(0x838c02, 0xea);
        theMMU.write8(0x838c03, 0xea);
        theMMU.write8(0x838c2c, 0xea);
        theMMU.write8(0x838c2d, 0xea);
        theMMU.write8(0x838bd4, 0xea);
        theMMU.write8(0x838bd5, 0xea);
        theMMU.write8(0x8388f9, 0xea);
        theMMU.write8(0x8388fa, 0xea);
        theMMU.write8(0x838954, 0xea);
        theMMU.write8(0x838955, 0xea);
        theMMU.write8(0x838966, 0xea);
        theMMU.write8(0x838967, 0xea);
        theMMU.write8(0x838974, 0xea);
        theMMU.write8(0x838975, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\James Pond's Crazy Sports (E).smc")
    {
        theMMU.write8(0x81f408, 0xea);
        theMMU.write8(0x81f409, 0xea);
        theMMU.write8(0x81f429, 0xea);
        theMMU.write8(0x81f42a, 0xea);
        theMMU.write8(0x81f44a, 0xea);
        theMMU.write8(0x81f44b, 0xea);
        theMMU.write8(0x81f46b, 0xea);
        theMMU.write8(0x81f46c, 0xea);
        theMMU.write8(0x81f55c, 0xea);
        theMMU.write8(0x81f55d, 0xea);

        //theMMU.write8(0x82f0b0, 0xea);
        //theMMU.write8(0x82f0b1, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Lemmings (E).sfc")
    {
        theMMU.write8(0x08d45a, 0xea);
        theMMU.write8(0x08d45b, 0xea);
        theMMU.write8(0x08d46c, 0xea);
        theMMU.write8(0x08d46d, 0xea);
        theMMU.write8(0x08d0f2, 0xea);
        theMMU.write8(0x08d0f3, 0xea);
        theMMU.write8(0x08d0fe, 0xea);
        theMMU.write8(0x08d0ff, 0xea);
        theMMU.write8(0x08d133, 0xea);
        theMMU.write8(0x08d134, 0xea);
        theMMU.write8(0x08d140, 0xea);
        theMMU.write8(0x08d141, 0xea);
        theMMU.write8(0x08d14d, 0xea);
        theMMU.write8(0x08d14e, 0xea);
        theMMU.write8(0x08d3b3, 0xea);
        theMMU.write8(0x08d3b4, 0xea);
        theMMU.write8(0x08d3cd, 0xea);
        theMMU.write8(0x08d3ce, 0xea);
        theMMU.write8(0x08d2c2, 0xea);
        theMMU.write8(0x08d2c3, 0xea);
        theMMU.write8(0x08d2dd, 0xea);
        theMMU.write8(0x08d2de, 0xea);
        theMMU.write8(0x08d396, 0xea);
        theMMU.write8(0x08d397, 0xea);
        theMMU.write8(0x08d07e, 0xea);
        theMMU.write8(0x08d07f, 0xea);
    }
    else if (romName == "d:\\prova\\snes\\Lawnmower Man, The (E).smc")
    {
        theMMU.write8(0x868135, 0xea);
        theMMU.write8(0x868136, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Williams Arcade's Greatest Hits (E) [!].smc")
    {
        theMMU.write8(0x8ce72d, 0xea);
        theMMU.write8(0x8ce72e, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Mickey Mania (E).smc")
    {
        theMMU.write8(0xa195dc, 0xea);
        theMMU.write8(0xa195dd, 0xea);
        theMMU.write8(0xa1980e, 0xea);
        theMMU.write8(0xa1980f, 0xea);
        
        theMMU.write8(0xb3fab2, 0xea);
        theMMU.write8(0xb3fab3, 0xea);
        theMMU.write8(0xb3fabf, 0xea);
        theMMU.write8(0xb3fac0, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Pinball Dreams (E).smc")
    {
        theMMU.write8(0x80d8cd, 0xea);
        theMMU.write8(0x80d8ce, 0xea);
        theMMU.write8(0x80d8d8, 0xea);
        theMMU.write8(0x80d8d9, 0xea);
        theMMU.write8(0x80d8dd, 0xea);
        theMMU.write8(0x80d8de, 0xea);
        theMMU.write8(0x80d8a0, 0xea);
        theMMU.write8(0x80d8a1, 0xea);
        theMMU.write8(0x80d8b5, 0xea);
        theMMU.write8(0x80d8b6, 0xea);
        theMMU.write8(0x80d933, 0xea);
        theMMU.write8(0x80d934, 0xea);
        theMMU.write8(0x80d957, 0xea);
        theMMU.write8(0x80d958, 0xea);
        theMMU.write8(0x80d9a3, 0xea);
        theMMU.write8(0x80d9a4, 0xea);
        theMMU.write8(0x80d9d2, 0xea);
        theMMU.write8(0x80d9d3, 0xea);
        theMMU.write8(0x80d9f6, 0xea);
        theMMU.write8(0x80d9f7, 0xea);
        
        theMMU.write8(0x894b, 0xea);
        theMMU.write8(0x894c, 0xea);
        theMMU.write8(0x8956, 0xea);
        theMMU.write8(0x8957, 0xea);
        theMMU.write8(0x8987, 0xea);
        theMMU.write8(0x8988, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Final Fight (U).smc")
    {
        theMMU.write8(0x181f5, 0xea);
        theMMU.write8(0x181f6, 0xea);
        theMMU.write8(0x18231, 0xea);
        theMMU.write8(0x18232, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Sensible Soccer - International Edition (E).smc")
    {
        theMMU.write8(0x808258, 0xea);
        theMMU.write8(0x808259, 0xea);
        theMMU.write8(0x809b94, 0xea);
        theMMU.write8(0x809b95, 0xea);
        theMMU.write8(0x80bcca, 0xea);
        theMMU.write8(0x80bccb, 0xea);
        theMMU.write8(0x80bceb, 0xea);
        theMMU.write8(0x80bcec, 0xea);
        theMMU.write8(0x80bd0c, 0xea);
        theMMU.write8(0x80bd0d, 0xea);
        theMMU.write8(0x80bd2d, 0xea);
        theMMU.write8(0x80bd2e, 0xea);
        
    }
    else if (romName == "d:\\prova\\snes\\Monopoly (V1.1) (U).smc")
    {
        theMMU.write8(0xfdb6, 0xea);
        theMMU.write8(0xfdb7, 0xea);

    }
    else if (romName == "D:\\prova\\snes\\HiRom\\Flashback - The Quest for Identity (U) [!].smc")
    {
        theMMU.write8(0xd900fd, 0xea);
        theMMU.write8(0xd900fe, 0xea);
        
        theMMU.write8(0xc15b60, 0xea);
        theMMU.write8(0xc15b61, 0xea);
        theMMU.write8(0xc159cf, 0xea);
        theMMU.write8(0xc159d0, 0xea);
        theMMU.write8(0xc15974, 0xea);
        theMMU.write8(0xc15975, 0xea);
    }
    else if (romName == "D:\\prova\\snes\\HiRom\\Donkey Kong Country (V1.1) (E).smc")
    {
        theMMU.write8(0x8ab51c, 0xea);
        theMMU.write8(0x8ab51d, 0xea);
        theMMU.write8(0x8ab20e, 0xea);
        theMMU.write8(0x8ab20f, 0xea);
        theMMU.write8(0x8ab1b9, 0xea);
        theMMU.write8(0x8ab1ba, 0xea);

    }
    else if (romName == "D:\\prova\\snes\\HiRom\\Final Fantasy III (USA).sfc")
    {
        theMMU.write8(0xc50055, 0xea);
        theMMU.write8(0xc50056, 0xea);
        
        theMMU.write8(0xc0b7a3, 0xea);
        theMMU.write8(0xc0b7a4, 0xea);

    }
    else if (romName == "D:\\romz\\nintendo\\snes\\Earthbound (U).smc")
    {
        theMMU.write8(0xc0abba, 0xea);
        theMMU.write8(0xc0abbb, 0xea);
        theMMU.write8(0xc0ab93, 0xea);
        theMMU.write8(0xc0ab94, 0xea);

    }
    else if (romName == "D:\\prova\\snes\\HiRom\\Earthworm Jim (U).smc")
    {
        theMMU.write8(0xc30654, 0xea);
        theMMU.write8(0xc30655, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Incredible Crash Dummies, The (U).smc")
    {
        theMMU.write8(0x9d8498, 0xea);
        theMMU.write8(0x9d8499, 0xea);
        theMMU.write8(0x9d85cd, 0xea);
        theMMU.write8(0x9d85ce, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Street Fighter II - The World Warrior (U).smc")
    {
        theMMU.write8(0x8b76, 0xea);
        theMMU.write8(0x8b77, 0xea);
        theMMU.write8(0x8b65, 0xea);
        theMMU.write8(0x8b66, 0xea);
        theMMU.write8(0x80fb, 0xea);
        theMMU.write8(0x80fc, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\R-Type 3 (U).smc")
    {
        theMMU.write8(0xaa820e, 0xea);
        theMMU.write8(0xaa820f, 0xea);
        theMMU.write8(0xaa822a, 0xea);
        theMMU.write8(0xaa822b, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Robocop 3 (U).smc")
    {
        theMMU.write8(0xe9cb, 0xea);
        theMMU.write8(0xe9cc, 0xea);
        theMMU.write8(0xe9dd, 0xea);
        theMMU.write8(0xe9de, 0xea);
        theMMU.write8(0xe9e4, 0xea);
        theMMU.write8(0xe9e5, 0xea);
        theMMU.write8(0x8130, 0xea);
        theMMU.write8(0x8131, 0xea);
        theMMU.write8(0x8296, 0xea);
        theMMU.write8(0x8297, 0xea);
    }
    else if (romName == "D:\\prova\\snes\\demos\\elix-nu-pal.sfc")
    {
        theMMU.write8(0x8241, 0xea);
        theMMU.write8(0x8242, 0xea);

    }
    else if (romName == "D:\\prova\\snes\\demos\\2.68 MHz Demo (PD).sfc")
    {
        theMMU.write8(0xce0f, 0xea);
        theMMU.write8(0xce10, 0xea);
        theMMU.write8(0xce62, 0xea);
        theMMU.write8(0xce63, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Micro Machines (U).smc")
    {
        theMMU.write8(0x1f76c, 0xea);
        theMMU.write8(0x1f76d, 0xea);
        theMMU.write8(0x1f780, 0xea);
        theMMU.write8(0x1f781, 0xea);
        theMMU.write8(0x1f78a, 0xea);
        theMMU.write8(0x1f78b, 0xea);
        theMMU.write8(0x1f791, 0xea);
        theMMU.write8(0x1f792, 0xea);
        theMMU.write8(0x1f700, 0xea);
        theMMU.write8(0x1f701, 0xea);
        
        //theMMU.write8(0x1f725, 0xea);
        //theMMU.write8(0x1f726, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Super Mario All-Stars + Super Mario World (USA).sfc")
    {
        /*theMMU.write8(0x886b, 0xea);
        theMMU.write8(0x886c, 0xea);
        theMMU.write8(0x882a, 0xea);
        theMMU.write8(0x882b, 0xea);
        theMMU.write8(0x8835, 0xea);
        theMMU.write8(0x8836, 0xea);
        theMMU.write8(0x883d, 0xea);
        theMMU.write8(0x883e, 0xea);
        theMMU.write8(0x8848, 0xea);
        theMMU.write8(0x8849, 0xea);
        theMMU.write8(0x8850, 0xea);
        theMMU.write8(0x8851, 0xea);
        theMMU.write8(0x8c21, 0xea);
        theMMU.write8(0x8c22, 0xea);
        */
        //theMMU.write8(0x8d65, 0x4c);
        //theMMU.write8(0x8d66, 0x36);
        //theMMU.write8(0x8d67, 0xa0);
        //theMMU.write8(0x8cac, 0xea);
        //theMMU.write8(0x8cad, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Out of This World (U).smc")
    {
        theMMU.write8(0x80e5fb, 0xea);
        theMMU.write8(0x80e5fc, 0xea);
        theMMU.write8(0x80e605, 0xea);
        theMMU.write8(0x80e606, 0xea);
        theMMU.write8(0x80e638, 0xea);
        theMMU.write8(0x80e639, 0xea);
        
        //theMMU.write8(0x808825, 0xea);
        //theMMU.write8(0x808826, 0xea);
        //theMMU.write8(0x80e2e9, 0xea);
        //theMMU.write8(0x80e2ea, 0xea);
    }
    else if (romName == "D:\\prova\\snes\\HiRom\\Super Bomberman (U).smc")
    {
        theMMU.write8(0xc484db, 0xea);
        theMMU.write8(0xc484dc, 0xea);
        theMMU.write8(0xc4850e, 0xea);
        theMMU.write8(0xc4850f, 0xea);
        theMMU.write8(0xc484f2, 0xea);
        theMMU.write8(0xc484f3, 0xea);

    }
    else if (romName == "D:\\prova\\snes\\HiRom\\Diddy's Kong Quest (E).smc")
    {
        theMMU.write8(0xb58414, 0xea);
        theMMU.write8(0xb58415, 0xea);
        theMMU.write8(0xb58426, 0xea);
        theMMU.write8(0xb58427, 0xea);
        theMMU.write8(0xb58431, 0xea);
        theMMU.write8(0xb58432, 0xea);

    }
    else if (romName == "d:\\prova\\snes\\Final Fantasy IV (J).smc")
    {
        theMMU.write8(0x9260, 0xea);
        theMMU.write8(0x9261, 0xea);
        theMMU.write8(0x9287, 0xea);
        theMMU.write8(0x9288, 0xea);

    }
    

    //

    debugger5a22 theDebugger5a22;
    cpu5a22 theCPU(&theMMU,false);
    theCPU.reset();
    theMMU.setCPU(theCPU);

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
    //unsigned char* selectedMemoryPortion = (unsigned char*)"VRAM";

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

        displayAppoWindow(thePPU,theMMU,theDebugger5a22);
        displayRomLoadingLogWindow(romLoadingLog);

        bool rush = false;
        int rushToAddress = 0;
        displayDebugWindow(theCPU, theDebugger5a22,theMMU,isDebugWindowFocused,rush,rushToAddress,jumpToAppoBuf,totCPUCycles,emustatus,thePPU);
        displayRegistersWindow(theCPU,thePPU,totCPUCycles);
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

                //if ( ((theCPU.getPC()) == 0x808a5a) && thePPU.getWriteBreakpoint() )
                //if ( ((theCPU.getPC()) == 0x808a4d) && (theCPU.getX()==0x14fb))
                //if ( ((theCPU.getPC()&0xffff) == 0x9e3b))
                if ( ((theCPU.getPC()&0xffff) == 0x8821))
                {
                    //emustatus = 0;
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

        // Rendering
        ImGui::Render();

        glViewport(0, 0, g_Width, g_Height);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        ::SwapBuffers(g_MainWindow.hDC);
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
