# MSX Game "NOBORUNOCA"

![](https://github.com/h1romas4/noborunoca/workflows/Build/badge.svg)

![](https://raw.githubusercontent.com/h1romas4/noborunoca/main/docs/noborunoca-01.png)

🎮 [Let's Play! by WebMSX](https://webmsx.org/?MACHINE=MSX2J&ROM=https://github.com/h1romas4/noborunoca/releases/download/v1.0.0/noborunoca.rom)

📼 [YouTube Video](https://www.youtube.com/watch?v=SFgWwkPuj2M)

## License

MIT License

Includes all source code and resources.

## Special Thanks

- [Z88DK - The Development Kit for Z80 Computers](https://github.com/z88dk/z88dk) - Compiler and toolchain
- [Lovely Composer](https://1oogames.itch.io/lovely-composer) - Music Sequencer
- [@aburi6800](https://github.com/aburi6800) - MSX Sound Driver and Convertor
- [MAME](https://www.mamedev.org/) - Emulator
- [openMSX](https://openmsx.org/) - Emulator
- [C-BIOS](http://cbios.sourceforge.net/) - MSX Open Source BIOS

## Build

### Require

- Ubuntu 20.04 LTS or Ubuntu 22.04 LTS or Windows WSL2
- Setup [Z88DK](https://github.com/z88dk/z88dk/wiki/installation#linux--unix)
- cmake (`sudo apt install cmake`)

Set enviroments on `~/.bashrc`

```
# z88dk
export Z88DK_HOME=/home/hiromasa/devel/msx/z88dk
export ZCCCFG=${Z88DK_HOME}/lib/config
export PATH=${Z88DK_HOME}/bin:${PATH}
```

Verifiy

```
$ which zcc
/home/hiromasa/devel/msx/z88dk/bin/zcc
$ ls -laF ${ZCCCFG}/msx.cfg
-rw-rw-r-- 1 hiromasa hiromasa 1035  9月  1 12:10 /home/hiromasa/devel/msx/z88dk/lib/config/msx.cfg
$ zcc 2>&1 | head -5
zcc - Frontend for the z88dk Cross-C Compiler - v18586-be7c8763a-20210901

Usage: zcc +[target] {options} {files}
   -v -verbose                  Output all commands that are run (-vn suppresses)
   -h -help                     Display this text
```

### Build

Compile

```
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/z88dk.cmake ..
make
```

Verifiy

```
ls -laF ../dist/*.rom
-rw-rw-r-- 1 hiromasa hiromasa  16384  9月  3 18:13 noborunoca.rom
```

## See also

> Z88DK MSX build template with sample game
>
> https://github.com/h1romas4/z88dk-msx-template
