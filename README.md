# Briefcase.StaminaControl

Mod serveur natif pour https://github.com/EnoPM/BriefcaseNative.
Sources, tests, versions et releases indépendants du framework.

## Compiler
Visual Studio C++ x64 et PowerShell 7 requis.
scripts/Build.ps1 télécharge le SDK défini dans mod-build.json, compile, teste et crée le ZIP.
Hors ligne : scripts/Build.ps1 -SdkPath CHEMIN_DU_SDK_EXTRAIT.
Les tests utilisént la vraie DLL avec un faux backend ABI, sans jeu ni UE4SS.

## Publier
Publier d'abord le SDK du framework, puis utilisér Actions -> Publish mod release -> Run workflow.
La version doit correspondre a CMakeLists.txt et briefcase.mod.json. Le mode brouillon est disponible.
Ne jamais remplacer une version deja distribuee.

## Installer
Serveur arrêté, extraire le ZIP de dist/ dans Win64 en conservant Data/config.json existant.
Activer le mod dans les réglages locaux de Briefcase si nécessaire.
Les identifiants, noms de DLL et configurations historiques sont conserves.
