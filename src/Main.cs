using UnityEngine;
using UnityModManagerNet;
using HarmonyLib;
using System.Reflection;

namespace RpgsCommunityPatch
{
    public class Main
    {
        // Entry point for the mod.
        static bool Load(UnityModManager.ModEntry modEntry)
        {
            var harmony = new Harmony(modEntry.Info.Id);
            harmony.PatchAll(Assembly.GetExecutingAssembly());

            modEntry.Logger.Log("Hello World!");

            return true;
        }
    }
}
