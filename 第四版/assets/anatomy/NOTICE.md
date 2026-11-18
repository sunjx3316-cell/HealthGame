# Anatomical model attribution and license

The files atlas.json and the body, heart, lungs, liver, stomach and skin meshes in this directory are derived anatomical models,
distributed under Creative Commons Attribution-ShareAlike 4.0 International
(CC BY-SA 4.0). This license applies to the anatomical assets, not as a new license
declaration for the surrounding application code.

Required upstream attributions:

- **BodyParts3D - The Database Center for Life Science - CC-BY-SA 2.1 Japan**
  Original model: Kousaku Okubo.
  https://dbarchive.biosciencedbc.jp/en/bodyparts3d/
  https://creativecommons.org/licenses/by-sa/2.1/jp/
- **Z-Anatomy - The libre 3D atlas of anatomy - CC-BY-SA 4.0**
  Design, anatomy and 3D work: Gauthier Kervyn.
  Upstream project also credits Marcin Zielinski (Blender add-on) and Lluis Vinent
  (Unity application). Their application/add-on code is not used by this exporter.
  https://github.com/Z-Anatomy/Models-of-human-anatomy
  https://creativecommons.org/licenses/by-sa/4.0/

Source archive:
https://raw.githubusercontent.com/Z-Anatomy/Models-of-human-anatomy/master/Z-Anatomy.zip
Retrieved 2026-09-07.
SHA-256: E029688545627BD0214B269E1063143ABB580AAD72B2C2445D6D8A9A0D9DA736

Changes in this adaptation:
selected anatomical structures; evaluated subdivision and curve geometry;
triangulated, converted coordinate axes, corrected mirrored winding, normalized
detail views with reversible registered placements; replaced presentation colors;
calculated geometric cavity shading; compressed into the HealthGame mesh format.
The stomach's pre-existing large teaching display window is reconstructed for
the intact view; a runtime cut plane provides the game's section instead.
The forearm's offset inner surface is a schematic dermal layer, not a histological
or measured skin sample. Age morphs, lesion deformation and treatment particles
are original game visualizations on these derived surfaces.
Gastric rugae relief is synthesized on a subdivided source mucosal surface and
is a schematic appearance, not a patient-specific histological reconstruction.

The exporter intentionally excludes the kidney and inner-ear reference models
listed by Z-Anatomy with NonCommercial licenses, and does not copy Wikipedia
definitions, brain reference models or the upstream Blender/Python application.

No endorsement by the upstream authors is implied. Models may contain errors.
These assets and their adaptations have not been clinically validated and are
not a medical device, diagnostic tool, pediatric atlas or treatment guide.
When redistributing modified anatomical assets, retain these attributions,
identify changes, and apply the share-alike license.

See CC-BY-SA-4.0.txt for the license. Each part's original anatomical name is
retained in atlas.json; reproducible conversion code is in scripts/export_anatomy.py.

The character exterior in portrait.mesh is documented separately in PORTRAIT-NOTICE.md.
