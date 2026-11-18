# HealthGame portrait assets

`portrait.mesh` contains the four registered shapes used for the character's
outer surface. `portrait.json` describes the draw ranges. The source shapes
are the MakeHuman HM08 base mesh and the asian-male baby, child, young and old
macro targets. MakeHuman released these core assets under CC0 1.0.

[MakeHuman asset license](https://github.com/makehumancommunity/makehuman/blob/master/LICENSE.md)
and [upstream data directory](https://github.com/makehumancommunity/makehuman/tree/master/makehuman/data).
Original target sculpting credits include Manuel Bastioni. The source files
credit Data Collection AB, Joel Palmius and Jonas Hauquier.

The game registers the four shapes to its anatomy coordinate frame, subdivides
them on corresponding vertices, and derives clothing, eyes and a fitted scalp
surface. Source data and SHA-256 fingerprints for reproducing the export:

| File under upstream data | SHA-256 |
| --- | --- |
| 3dobjs/base.obj | 8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c |
| targets/macrodetails/asian-male-baby.target | 296f9dd9c9716a0e43afb9dadfb35e11be0ecc1f15d2efae6e903b0e91bc07a5 |
| targets/macrodetails/asian-male-child.target | c1b56e9cc549cd7c463f407557d7adceaa217b528c600d34c19df98bc3d6472e |
| targets/macrodetails/asian-male-young.target | ed2e8c191cb6b87b4a2d97c80486acb2604fa549316c7aa2a738a5d5a14334dc |
| targets/macrodetails/asian-male-old.target | 36dc5a3a135ded33885e814806b0e3668ca5bf44bddcebdee48ae75ae68bc3d3 |

Place the five files in a directory and run
`python scripts/export_portrait.py SOURCE_DIR 第四版/assets/anatomy` with NumPy installed.
The separate Z-Anatomy / BodyParts3D organ and skeleton meshes retain the
attributions and share-alike license in NOTICE.md.
