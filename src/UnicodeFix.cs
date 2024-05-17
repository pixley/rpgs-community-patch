using HarmonyLib;
using AudioStream;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using System.Linq;
using System;
using System.Runtime.InteropServices;

namespace RpgsCommunityPatch
{
    [HarmonyPatch(typeof(AudioStreamBase), "ReadTags")]
    public static class UnicodeFix
    {
        static IEnumerable<CodeInstruction> Transpiler(IEnumerable<CodeInstruction> instructions)
        {
            // We need to change tag reading so that Unicode tags don't get read as ANSI
            List<CodeInstruction> codes = new List<CodeInstruction>(instructions);

            bool injectionAttempted = false;

            MethodInfo PtrToStringAnsiMethod = typeof(Marshal).GetMethod("PtrToStringAnsi", new Type[] { typeof(IntPtr), typeof(int) });
            for (int i = 0; i < codes.Count; i++)
            {
                if (codes[i].opcode == OpCodes.Call)
                {
                    if (codes[i].operand as MethodInfo == PtrToStringAnsiMethod)
                    {
                        codes[i].operand = typeof(Marshal).GetMethod("PtrToStringUni", new Type[] { typeof(IntPtr), typeof(int) });

                        injectionAttempted = true;
                    }
                }
            }

            if (injectionAttempted)
            {
                Main.Log($"Unicode metadata fix applied.");
                return codes.AsEnumerable();
            }
            else
            {
                Main.Log($"Unicode metadata fix failed.");
                return instructions;
            }
        }
    }
}
