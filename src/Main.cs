using UnityModManagerNet;
using HarmonyLib;
using System.Reflection;
using System.IO;

namespace RpgsCommunityPatch
{
    static class Main
    {
        private static UnityModManager.ModEntry mod;
        private static Harmony harmony;

        // Entry point for the mod.
        static bool Load(UnityModManager.ModEntry modEntry)
        {
            harmony = new Harmony(modEntry.Info.Id);
            mod = modEntry;

            mod.OnToggle = OnToggle;

            return true;
        }

        // Controls enable/disable
        static bool OnToggle(UnityModManager.ModEntry modEntry, bool value)
        {
            if (value)
            {
                OnEnable();
            }
            else
            {
                OnDisable();
            }

            return true;
        }

        static void OnEnable()
        {
            harmony.PatchAll(Assembly.GetExecutingAssembly());

            string version = GetVersionString();
            Log($"RPG Sounds Community Patch version {version} loaded.");
        }

        static void OnDisable()
        {
            harmony.UnpatchAll(harmony.Id);

            // Perform any cleanup
            CodecLoader.CleanupPlugin();

            Log("RPG Sounds Community Patch disabled.");
        }

        public static string GetVersionString()
        {
            // ModEntry contains the version info pulled from Info.json
            return mod.Version.ToString();
        }

        public static void Log(string logString)
        {
            mod.Logger.Log(logString);
        }

        public static string GetModDirectory()
        {
            return mod.Path;
        }
    }
}
