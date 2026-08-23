// Copyright 2026 Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class TrackMark : ModuleRules
{
	public TrackMark(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"RenderCore",
			// UPhysicalMaterial lives here. Engine pulls it in transitively, but the surface lookup is a
			// first-class part of this module, so the dependency is spelled out rather than inherited.
			"PhysicsCore",
		});

		// Deliberately absent: UMG, Niagara, AIModule. The stats overlay draws on UCanvas so it survives a
		// cooked Shipping build, and nothing here needs a particle system or a navigation query.
	}
}
