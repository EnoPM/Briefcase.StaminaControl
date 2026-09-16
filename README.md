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
La version se configure uniquement dans `VERSION`. CMake genere le manifeste a partir de `briefcase.mod.json.in`.
Un push sur `main` modifiant `VERSION` compile, teste et publie les deux plateformes dans ce depot prive.
Le workflow manuel utilise aussi `VERSION` et conserve une option brouillon. Une release existante n'est jamais remplacee.
Le workflow compile et teste Windows et Linux, puis publie les deux ZIP et leurs SHA-256 dans une seule release. Il refuse de publier si le dépôt n'est pas privé. Le SDK 0.5.0 est épinglé par son empreinte dans mod-build.json.
Ne jamais remplacer une version deja distribuee.

## Installer
Serveur arrêté, extraire le ZIP de dist/ dans Win64 en conservant Data/config.json existant.
Sur Linux, choisir le ZIP linux-x64 et l'extraire dans Binaries/Linux. BriefcaseNative 0.5.0 est requis pour cette release. Aucun interpréteur n'est nécessaire à l'exécution du mod.
Activer le mod dans les réglages locaux de Briefcase si nécessaire.
Les identifiants, noms de DLL et configurations historiques sont conserves.

## Linux server build

Use the public SDK built with Linux support (including the appended startup code-window service). A Windows DLL cannot run on Linux.

~~~sh
python3 scripts/build-linux.py --sdk /path/to/extracted/BriefcaseNative-SDK
~~~

Requires Ubuntu 24.04 x64, Clang 19, CMake 3.28, Ninja and Python 3. The script builds and tests the mod, generates its .so manifest and writes a separate linux-x64 ZIP and checksum under dist. Configuration remains under the same mod ID and Data/config.json.
