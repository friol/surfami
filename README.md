# surFami, a SNES (Super Nintendo Entertainment System) emulator

SNES has always fascinated me. I've never owned it. My parent's approach to what contains electronics inside was too demonizing; moreover, we had only one television at home, and its only purpose was to show a movie in the evening and only on the national channels. I think that mobile phones are a different matter, today, and their size and approachable cost is the real trojan horse for their mass diffusion. Anyway, my parents would have never bought something whose unique use was to play videogames (videogames are evil, you know. This is probably the reason why I'm still playing them now that I am 47).<br/><br/>

So, with those premises, i *had* to write a SNES emulator.<br/><br/>

SNES is not that hard to emulate, at the end. Beware, emulating it accurately would require a whole lot of effort (years, if not decades, if you think of something like https://github.com/higan-emu/higan).<br/>
That's not the aim of surFami. I just wanted to hack together an app to demonstrate that in a few months you can create a working SNES emu. I think I succeeded.<br/>
I've spent about 2 months and a half on surFami (as you can see from the commit history), and, as always, working on it only in my free time.
<br/><br/>
The 2 CPUs (65816 and SPC700) in surFami are built with Tom Harte's CPU tests in mind. You can test each opcode with those tests. This helped immensely to build working CPUs and to lose the least amount of time running behind CPU bugs. Building the emu for the rest of the hardware (DMAs, PPU, MMU, etc.) was harder and a lot of bugs have been fixed in the process. I'm still not emulating a couple of things (open bus) and emulating badly a couple of other things. But, you know, I can feel satisfied for 2.5 months of work.
<br/><br/>
You can find below a "visual history" of the surFami builds that represents the growth of the emu.
<br/><br/>

The first Krom tests:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20231229.png" width="480" /><br/>

The SNES Nintendo test rom was one of the first roms to boot:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240105.png" width="480" /><br/>

Nintendo sprites in Super Mario World:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240107.png" width="480" /><br/>

A very messy Super Mario World title screen:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240107_2.png" width="480" /><br/>

Colorful title screen:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240111.png" width="480" /><br/>

Super Mario World going ingame:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240114.png" width="480" /><br/>

Parodius going ingame:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240124.png" width="480" /><br/>

The triforce of Zelda, available only implementing the inner multiplication in hardware:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240201.png" width="480" /><br/>

Donkey Kong Country was the first HiRom to boot:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240204.png" width="480" /><br/>

DKC ingame:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240209.png" width="480" /><br/>

HDMA effects in a demoscene release:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240210.png" width="480" /><br/>

F-Zero titlescreen correctly displayed after implementing the VIRQ (it switches between modes 1 and 7):<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240212.png" width="480" /><br/>

Wolfenstein working with HDMA:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240214.2.png" width="480" /><br/>

A slightly glitched title screen of The Addams Family, after fixing a DMA quirk:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240214.png" width="480" /><br/>

Super Mario All-stars was hard to run because of missing SRAM implementation:<br/>
<img src="https://github.com/friol/surfami/blob/master/images/ss20240219.2.png" width="480" /><br/>

<br/>
What next?
