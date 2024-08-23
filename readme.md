# RPG Sounds Community Patch

This mod applies various fixes and updates for [RPG Sounds](https://store.steampowered.com/app/1480140/RPG_Sounds/), a free Unity-based audio mixer intended for use in tabletop RPGs.  RPG Sounds development ceased in early 2022, but the application has a lot of potential.  This community patch seeks to ensure that people can still make use of it.

## Current Features

- Added support for MPEG-4 and Windows Media Audio audio files (.m4a and .wma)
- Fixed issue where Unicode metadata would cause tracks to fail to play properly
- Fixed saving of audio output device setting
- Scroll sensitivity increased

## Planned Features

- Ability to select separate audio output for previewing tracks in the Library without changing the output of the main mixer

This was initially developed to address Pixley's specific gripes with RPG Sounds.  There are community complaints about the Online Play functionality, and pull requests to address them are welcome.  Of course, pull requests to make other fixes or add improvements are also welcome.

## What This Is Not

The community patch is meant to fix broken functionality or add blatantly missing features.  Making radical changes to RPG Sounds is outside the scope of this mod.  However, it is intended for this repository to be a core point-of-reference for people looking to mod RPG Sounds.

## Requirements

The RPG Sounds Community Patch has only been developed and tested against Windows 10 and 11 (64-bit).  However, RPG Sounds itself does support macOS (though it doesn't seem to have made the jump to Apple Silicon).  Pull requests for macOS development are welcome!

Download [Unity Mod Manager](https://www.nexusmods.com/site/mods/21) and use it to install the ZIP file to your RPG Sounds installation.  You can find the Community Patch ZIP on the [Releases page](https://github.com/pixley/rpgs-community-patch/releases) or on [Nexus Mods](https://www.nexusmods.com/site/mods/1017).

## Development Environment

The rpgs-community-patch solution requires Visual Studio 2019 (any edition), no earlier, no later.  RPG Sounds is based on a version of Unity that uses .NET Framework 4.0, and VS2019 is the last version of Visual Studio to support it.  Note that this is **Visual Studio**, not Visual Studio Code.

To make use of the automatic install-after-build script, install PowerShell Core 7.0 or newer.

To build the MPEG-4 support library fmod_win32_mf, you will need to download the [FMOD Engine](https://www.fmod.com/download#fmodengine), version 2.01.07.  This is free software, but you do have to register for an FMOD account.  Once that is installed, copy `[FMOD install]/FMOD Studio API Windows/api/core/lib/x64/fmod_vc.lib` to `[repo directory]/fmod_win32_mf/lib`.  This is free software, but you do have to register for an FMOD account.

This repository provides reference assemblies for relevant packages used by RPG Sounds and Unity Mod Manager.  As reference assemblies, they do not actually contain any unlicensed software, instead only providing the API.

It is recommended to make use of a C# disassembler/decompiler such as [ILSpy](https://github.com/icsharpcode/ILSpy) or [dnSpy](https://github.com/dnSpy/dnSpy) to reference the internals of RPG Sounds code when developing.

Unfortunately, RPG Sounds has proven quite resistant to having debuggers attached to it, particularly at launch.  When launched outside of Steam, the app will restart with Steam attached, which sheds debuggers.  Investigation welcome.

Unity modding makes heavy use of [Harmony](https://harmony.pardeike.net/), a runtime code injection library for C#.  As such, prospective developers should reference its documentation.

## License

The RPG Sounds Community Patch is distributed under an MIT license.  See `license.txt` for details.