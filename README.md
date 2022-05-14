# "NOBORUNOCA" for MSX

[Playable by WebMSX](https://webmsx.org/cbios/?MACHINE=MSX2J&ROM=https://github.com/h1romas4/noborunoca/releases/download/v0.7.0/noborunoca.rom)

## License

BSD 3-Clause License

## Special Thanks

- [Z88DK - The Development Kit for Z80 Computers](https://github.com/z88dk/z88dk) - Compiler and toolchain
- [Lovely Composer](https://1oogames.itch.io/lovely-composer) - Music Sequencer
- [@aburi6800](https://github.com/aburi6800) - MSX Sound Driver and Convertor
- [MAME](https://www.mamedev.org/) - Emulator
- [openMSX](https://openmsx.org/) - Emulator
- [C-BIOS](http://cbios.sourceforge.net/) - MSX Open Source BIOS

## Build

### Require

- Ubuntu 20.04 LTS or Windows WSL2
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
