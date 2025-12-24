# MSX Game "NOBORUNOCA"

![](https://github.com/h1romas4/noborunoca/workflows/Build/badge.svg) ![](https://github.com/h1romas4/noborunoca/workflows/Release/badge.svg)

![](https://raw.githubusercontent.com/h1romas4/noborunoca/main/docs/noborunoca-01.png)

🎮 [Let's Play! by WebMSX](https://webmsx.org/?MACHINE=MSX2J&ROM=https://github.com/h1romas4/noborunoca/releases/download/v1.0.1/noborunoca.rom)

📼 [YouTube Video](https://www.youtube.com/watch?v=SFgWwkPuj2M)

## License

MIT License

This project includes all source code and resources.

## Special Thanks

- [Z88DK - The Development Kit for Z80 Computers](https://github.com/z88dk/z88dk) - Compiler and toolchain
- [Lovely Composer](https://1oogames.itch.io/lovely-composer) - Music sequencer
- [@aburi6800](https://github.com/aburi6800) - MSX sound driver and converter
- [MAME](https://www.mamedev.org/) - Emulator
- [openMSX](https://openmsx.org/) - Emulator
- [C-BIOS](http://cbios.sourceforge.net/) - MSX open-source BIOS

## Build

### Requirements

- Ubuntu 24.04 LTS or Ubuntu 22.04 LTS or Windows WSL2
- Z88DK v2.4
    - Setup [Z88DK](https://github.com/z88dk/z88dk/wiki/installation#linux--unix)
    - [.github/workflows/build-release.yml](https://github.com/h1romas4/noborunoca/blob/82297c63100820b234ac9c79967305e776f040ac/.github/workflows/build-release.yml#L28-L45)
- cmake (`sudo apt install cmake`)

Set environment variables in `~/.bashrc`:

```
# z88dk
export Z88DK_HOME=/home/hiromasa/devel/msx/z88dk
export ZCCCFG=${Z88DK_HOME}/lib/config
export PATH=${Z88DK_HOME}/bin:${PATH}
```

Verify

```
$ which zcc
/home/hiromasa/devel/msx/z88dk/bin/zcc
$ ls -laF ${ZCCCFG}/msx.cfg
-rw-rw-r-- 1 hiromasa hiromasa 1627 12月 22 20:18 /home/hiromasa/devel/msx/z88dk/lib/config/msx.cfg
$ zcc 2>&1 | head -5
zcc - Frontend for the z88dk Cross-C Compiler - v23854-4d530b6eb7-20251222

Usage: zcc +[target] {options} {files}
   -v -verbose                  Output all commands that are run (-vn suppresses)
   -h -help                     Display this text
```

### Build

Compile

```
cmake -S . -B build
cmake --build build
```

Verify

```
ls -laF ./dist/*.rom
-rw-rw-r-- 1 hiromasa hiromasa 32768 12月 23 17:07 ./dist/noborunoca.rom
```

## See also

> Z88DK MSX build template with sample game
>
> https://github.com/h1romas4/z88dk-msx-template
