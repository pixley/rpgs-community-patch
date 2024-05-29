using HarmonyLib;
using System.Reflection;
using System.Collections.Generic;
using System.Linq;
using System;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.IO;

namespace RpgsCommunityPatch
{
    [HarmonyPatch(typeof(DesktopFileImporter), "OpenImporter")]
    public static class NewFormats
    {
        static void Prefix(ref string[] extensions)
        {
            // Intercepting the array of extensions passed into the system open dialog to add our own

            // C# arrays are immutable, so we need to convert to list, add our stuff, then convert back
            List<string> extensionList = extensions.ToList();

            extensionList.Add("m4a");
            extensionList.Add("wma");

            extensions = extensionList.ToArray();
        }

        static void Prepare(MethodBase original)
        {
            if (original != null)
            {
                Main.Log("Additional audio file extensions now available.  Successful codec load required for playback of the additional formats.");
            }
        }
    }

    [HarmonyPatch(typeof(AudioStream.FMODSystem), MethodType.Constructor, new Type[] { typeof(FMOD.SPEAKERMODE), typeof(int), typeof(bool), typeof(bool) })]
    public static class CodecLoader
    {
        private static void Prepare(MethodBase original)
        {
            if (original != null)
            {
                bool nativeLogRegistered = false;
                try
                {
                    logCallback callbackInstance = new logCallback(OnLogCallback);
                    callbackHandle = GCHandle.Alloc(callbackInstance);
                    nativeLogRegistered = RegisterLogCallback(callbackInstance);
                }
                catch
                {
                    nativeLogRegistered = false;
                    callbackHandle.Free();
                }

                if (!nativeLogRegistered)
                {
                    Main.Log("Failed to register codec native logging callback.");
                }

                if (FmodSystemsWithCodec == null)
                {
                    FmodSystemsWithCodec = new List<Tuple<FMOD.System, uint>>();
                }
            }
        }

        private static void Postfix(AudioStream.FMODSystem __instance)
        {
            FMOD.System system = __instance.System;

            if (!system.hasHandle())
            {
                Main.Log("Cannot apply codec to handleless FMOD system!");
                return;
            }

            uint codecHandle = 0;
            string pluginPath = Path.Combine(Main.GetModDirectory(), "fmod_win32_mf.dll");

            Main.Log($"Loading plugin from {pluginPath}");

            FMOD.RESULT result = system.loadPlugin(pluginPath, out codecHandle, 2600);
            if (result == FMOD.RESULT.OK)
            {
                Main.Log($"Win32 Media Foundation codec registered to FMOD system with handle {codecHandle}.");
                FmodSystemsWithCodec.Add(new Tuple<FMOD.System, uint>(system, codecHandle));
            }
            else
            {
                Main.Log($"Failed to load Win32 Media Foundation codec: {result}");
            }
        }

        public static void CleanupPlugin()
        {
            foreach (Tuple<FMOD.System, uint> systemAndHandle in FmodSystemsWithCodec)
            {
                systemAndHandle.Item1.unloadPlugin(systemAndHandle.Item2);
            }
            FmodSystemsWithCodec.Clear();

            callbackHandle.Free();
        }

        private static List<Tuple<FMOD.System, uint>> FmodSystemsWithCodec;

        [DllImport("fmod_win32_mf", CallingConvention = CallingConvention.StdCall)]
        private static extern uint RegisterCodec(IntPtr FmodSystem);
        [DllImport("fmod_win32_mf", CallingConvention = CallingConvention.StdCall)]
        private static extern void UnregisterCodec(IntPtr FmodSystem);

        [DllImport("fmod_win32_mf", CallingConvention = CallingConvention.StdCall)]
        private static extern bool RegisterLogCallback(logCallback cb);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        private delegate void logCallback(IntPtr message, int size);
        private static void OnLogCallback(IntPtr message, int size)
        {
            string logString = Marshal.PtrToStringAnsi(message, size);

            Main.Log(logString);
        }
        // Make sure the callback stays valid forever
        private static GCHandle callbackHandle;
    }
}
