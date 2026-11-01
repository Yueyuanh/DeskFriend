"""Prepare ESP-DL model packages before the ESP-IDF Ninja build starts.

PlatformIO's ESP-IDF builder can invoke Ninja before the managed component
custom commands that pack/embed models have materialized their outputs.  The
official component CMake files then reference missing ``*.espdl.S`` files.
Generate those deterministic files up front so both clean and incremental
builds have the same inputs.
"""

from pathlib import Path
import subprocess

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
managed = project_dir / "managed_components"
espdl = managed / "espressif__esp-dl"
pack_script = espdl / "fbs_loader" / "pack_espdl_models.py"
embed_script = espdl / "fbs_loader" / "cmake" / "data_file_embed_asm_aligned.cmake"

# Use the tools of whichever PlatformIO core is running this build, so the
# project does not need a private copy of the toolchain next to it.
python_exe = Path(env.subst("$PYTHONEXE"))
cmake_exe = Path(env.subst("$PROJECT_PACKAGES_DIR")) / "tool-cmake" / "bin" / "cmake.exe"

for tool, path in (("python", python_exe), ("cmake", cmake_exe)):
    if not path.exists():
        raise SystemExit(f"prebuild_models: {tool} not found at {path}")

model_sets = {
    "pedestrian_detect": [
        managed / "espressif__pedestrian_detect" / "models" / "s3" / "pedestrian_detect_pico_s8_v1.espdl",
    ],
    "human_face_detect": [
        managed / "espressif__human_face_detect" / "models" / "s3" / "human_face_detect_msr_s8_v1.espdl",
        managed / "espressif__human_face_detect" / "models" / "s3" / "human_face_detect_mnp_s8_v1.espdl",
    ],
    "human_face_feat": [
        managed / "espressif__human_face_recognition" / "models" / "s3" / "human_face_feat_mfn_s8_v1.espdl",
    ],
}

packed_dir = build_dir / "espdl_models"
packed_dir.mkdir(parents=True, exist_ok=True)

for name, models in model_sets.items():
    packed = packed_dir / f"{name}.espdl"
    subprocess.run(
        [str(python_exe), str(pack_script), "--model_path", *(str(p) for p in models), "--out_file", str(packed)],
        check=True,
        cwd=project_dir,
    )
    source = build_dir / f"{name}.espdl.S"
    subprocess.run(
        [
            str(cmake_exe),
            "-D",
            f"DATA_FILE={packed}",
            "-D",
            f"SOURCE_FILE={source}",
            "-D",
            "FILE_TYPE=BINARY",
            "-P",
            str(embed_script),
        ],
        check=True,
        cwd=build_dir,
    )

