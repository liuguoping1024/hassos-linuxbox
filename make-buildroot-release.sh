#!/bin/bash
#
# Build ThirdReality LinuxBox HubV3 firmware images.
#
# Each board gets its own buildroot output tree (output/<board>) because the
# boards do not all share a toolchain: hubv3l builds against 5.4 kernel
# headers with a vendor 5.4 kernel and u-boot 2015.01, while the mainline
# boards use 6.6.120 and u-boot 2024.01. Sharing one output tree silently
# produces a toolchain with the wrong kernel headers.
#
# Does not require root: /cache is expected to be writable by the build user.
#
set -uo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${REPO_DIR}" || exit 1

# ---------------------------------------------------------------------------
# Environment hygiene
#
# buildroot's support/dependencies/dependencies.sh refuses to build if PATH or
# LD_LIBRARY_PATH contains an empty or "." entry (it reads as "current working
# directory", which breaks host tool builds). A stray "::" is easy to pick up
# from /etc/profile.d fragments. This used to be hidden because the script was
# run under sudo, whose default env_reset drops LD_LIBRARY_PATH entirely.
# Strip only the offending entries, preserving order.
# ---------------------------------------------------------------------------
strip_empty_path_entries() {
    printf '%s' "$1" | tr ':' '\n' | grep -vxE '\.?' | paste -sd: -
}

for _var in LD_LIBRARY_PATH PATH; do
    _old="${!_var:-}"
    [ -n "${_old}" ] || continue
    _new="$(strip_empty_path_entries "${_old}")"
    if [ "${_new}" != "${_old}" ]; then
        echo "--- ${_var}: removed empty/'.' entries (buildroot rejects them) ---"
        export "${_var}=${_new}"
    fi
done
unset _var _old _new

# ---------------------------------------------------------------------------
# Board table
#
#   name : defconfig : line : ram : flash : radio
#
# line   mainline = 6.6.120 + u-boot 2024.01 (tracks HAOS upstream / armbian)
#        amlogic-sdk    = Amlogic A113X vendor SDK, 5.4.180 + u-boot 2015.01
# ---------------------------------------------------------------------------
BOARDS=(
    "hubv3:thirdreality_hubv3_defconfig:mainline:2G:8G:zigbee+thread"
    "hubv3b:thirdreality_hubv3b_defconfig:mainline:2G:8G:zigbee+thread"
    "hubv3c:thirdreality_hubv3c_defconfig:mainline:2G:32G:zigbee+thread"
    "hubv3a:thirdreality_hubv3a_defconfig:amlogic-sdk:1G:8G:zigbee"
    "hubv3l:thirdreality_hubv3l_defconfig:amlogic-sdk:1G:8G:zigbee+thread"
)

# Bare-metal aarch64-elf toolchain, required only by the amlogic-sdk line
# (Amlogic u-boot 2015.01 / BL30 / BL301). Override in the environment.
AML_BAREMETAL_TOOLCHAIN="${AML_BAREMETAL_TOOLCHAIN:-/opt/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-elf}"
export AML_BAREMETAL_TOOLCHAIN

BR2_EXTERNAL_DIR="${REPO_DIR}/buildroot-external"
BUILDROOT_DIR="${REPO_DIR}/buildroot"
LOG_DIR="${REPO_DIR}/logs"

# ---------------------------------------------------------------------------
# Args
# ---------------------------------------------------------------------------
BOARD_ARG="hubv3"
JOBS="$(nproc)"
CLEAN_MODE="incremental"   # incremental | rootfs | full

board_names() { printf '%s\n' "${BOARDS[@]}" | cut -d: -f1; }

board_field() {  # board_field <name> <1-based field>
    local n="$1" f="$2" row
    for row in "${BOARDS[@]}"; do
        [ "${row%%:*}" = "$n" ] && { echo "$row" | cut -d: -f"$f"; return 0; }
    done
    return 1
}

usage() {
    cat <<EOF
Usage: $0 [-b BOARDS] [-j N] [--rootfs | clean] [-l] [-h]

  -b BOARDS   comma-separated board list, or "all"   (default: ${BOARD_ARG})
  -j N        parallel jobs                          (default: nproc = $(nproc))
  -l          list boards and exit
  -h          this help

Rebuild scope (pick at most one):
  (default)   incremental — let buildroot decide what to redo. Fastest.
              Caveat: a file DELETED from a rootfs overlay stays in the
              previous target/, so use --rootfs after removing overlay files.
  --rootfs    drop target/, images/ and all non-host package build dirs, then
              rebuild. Keeps the toolchain. This is what the old script did on
              every run.
  clean       remove output/<board> entirely (toolchain included).

Environment:
  AML_BAREMETAL_TOOLCHAIN   aarch64-elf toolchain root, amlogic-sdk line only
                            current: ${AML_BAREMETAL_TOOLCHAIN}

Output:
  output/<board>/            buildroot tree (per board, not shared)
  output/<board>/images/     artifacts
  logs/<board>-<ts>.log      build log

Examples:
  $0 -b hubv3a
  $0 -b hubv3a,hubv3l
  $0 -b all -j 32
  $0 -b hubv3l clean
  $0 -b hubv3b --rootfs
EOF
    exit "${1:-1}"
}

list_boards() {
    printf "%-8s %-34s %-9s %-6s %-6s %s\n" BOARD DEFCONFIG LINE RAM FLASH RADIO
    local row
    for row in "${BOARDS[@]}"; do
        IFS=: read -r n d l r f rad <<<"$row"
        printf "%-8s %-34s %-9s %-6s %-6s %s\n" "$n" "$d" "$l" "$r" "$f" "$rad"
    done
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        -b) [ $# -ge 2 ] || usage; BOARD_ARG="$2"; shift 2 ;;
        -j) [ $# -ge 2 ] || usage; JOBS="$2"; shift 2 ;;
        --rootfs) CLEAN_MODE="rootfs"; shift ;;
        clean)    CLEAN_MODE="full";   shift ;;
        -l) list_boards ;;
        -h|--help) usage 0 ;;
        *) echo "Unknown argument: $1" >&2; usage ;;
    esac
done

if [ "${BOARD_ARG}" = all ]; then
    mapfile -t SELECTED < <(board_names)
else
    IFS=, read -r -a SELECTED <<<"${BOARD_ARG}"
fi

# Validate board names up front so a typo fails before any work is done.
for b in "${SELECTED[@]}"; do
    if ! board_field "$b" 1 >/dev/null; then
        echo "Error: unknown board '$b'" >&2
        echo "Known boards: $(board_names | tr '\n' ' ')" >&2
        exit 1
    fi
done

[[ "${JOBS}" =~ ^[0-9]+$ ]] && [ "${JOBS}" -ge 1 ] || { echo "Error: -j needs a positive integer" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Prerequisites
# ---------------------------------------------------------------------------
init_submodule() {
    local subdir_count=0
    [ -d "${BUILDROOT_DIR}" ] && \
        subdir_count=$(find "${BUILDROOT_DIR}" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l)

    if [ ! -d "${BUILDROOT_DIR}" ] || [ "${subdir_count}" -lt 2 ]; then
        echo "--- buildroot submodule incomplete (subdirs: ${subdir_count}), initializing ---"
        git submodule update --init --recursive
        git submodule sync
    else
        echo "--- buildroot submodule present (subdirs: ${subdir_count}) ---"
    fi
}

# Patches against the buildroot submodule itself (python3, nodejs, ...).
# A patch that neither applies nor is already applied is a hard error; the
# previous version treated "already applied" and "conflict" as the same thing
# and silently continued.
apply_buildroot_patches() {
    local patch_dir="${BR2_EXTERNAL_DIR}/patches/buildroot"
    [ -d "${patch_dir}" ] || { echo "--- no buildroot submodule patches ---"; return 0; }

    echo "--- applying buildroot submodule patches ---"
    local pf name rc=0
    for pf in "${patch_dir}"/*.patch; do
        [ -f "$pf" ] || continue
        name="$(basename "$pf")"
        if git -C "${BUILDROOT_DIR}" apply --check "$pf" 2>/dev/null; then
            git -C "${BUILDROOT_DIR}" apply "$pf" && echo "    applied : ${name}"
        elif git -C "${BUILDROOT_DIR}" apply --reverse --check "$pf" 2>/dev/null; then
            echo "    already : ${name}"
        else
            echo "    CONFLICT: ${name}" >&2
            rc=1
        fi
    done
    if [ "$rc" -ne 0 ]; then
        echo "" >&2
        echo "One or more buildroot patches neither apply nor are already applied." >&2
        echo "Resolve manually in ${BUILDROOT_DIR} before rebuilding." >&2
        return 1
    fi
}

check_line_prereqs() {
    local line="$1"
    if [ "${line}" = amlogic-sdk ]; then
        if [ ! -x "${AML_BAREMETAL_TOOLCHAIN}/bin/aarch64-elf-gcc" ]; then
            echo "Error: amlogic-sdk boards need a bare-metal aarch64-elf toolchain." >&2
            echo "  looked for: ${AML_BAREMETAL_TOOLCHAIN}/bin/aarch64-elf-gcc" >&2
            echo "  set AML_BAREMETAL_TOOLCHAIN=/path/to/gcc-linaro-<ver>-aarch64-elf" >&2
            return 1
        fi
    fi
    # BR2_DL_DIR / BR2_CCACHE_DIR in the defconfigs point at /cache.
    for d in /cache/dl /cache/cc; do
        if ! mkdir -p "$d" 2>/dev/null; then
            echo "Error: cannot create ${d} (required by BR2_DL_DIR/BR2_CCACHE_DIR)." >&2
            echo "  run once: sudo install -d -o \"\$(id -u)\" -g \"\$(id -g)\" /cache/dl /cache/cc" >&2
            return 1
        fi
    done
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
build_board() {
    local board="$1"
    local defconfig line out log ts
    defconfig="$(board_field "$board" 2)"
    line="$(board_field "$board" 3)"
    out="${REPO_DIR}/output/${board}"
    ts="$(date +%Y%m%d-%H%M%S)"
    log="${LOG_DIR}/${board}-${ts}.log"

    echo ""
    echo "======================================================================"
    printf "  board %s   line %s   ram %s   flash %s   radio %s\n" \
        "$board" "$line" "$(board_field "$board" 4)" \
        "$(board_field "$board" 5)" "$(board_field "$board" 6)"
    echo "  defconfig $defconfig"
    echo "  output    output/${board}"
    echo "  jobs      ${JOBS}"
    echo "  scope     ${CLEAN_MODE}"
    echo "  log       logs/${board}-${ts}.log"
    echo "======================================================================"

    check_line_prereqs "${line}" || return 1

    case "${CLEAN_MODE}" in
        full)
            echo "--- removing output/${board} ---"
            rm -rf "${out}"
            ;;
        rootfs)
            if [ -d "${out}" ]; then
                echo "--- dropping target/, images/ and non-host build dirs ---"
                rm -rf "${out}/target" "${out}/images"
                find "${out}/build" -maxdepth 1 -mindepth 1 -type d \
                    ! -name 'host-*' -exec rm -rf {} + 2>/dev/null || true
                rm -rf "${out}"/build/host-uboot-tools-* 2>/dev/null || true
                find "${out}/build" -maxdepth 1 -type f -delete 2>/dev/null || true
            fi
            ;;
        incremental) : ;;
    esac

    mkdir -p "${out}" "${LOG_DIR}"

    {
        echo "### $(date -Iseconds)  board=${board} defconfig=${defconfig} line=${line}"
        echo "### git $(git rev-parse --short HEAD) ($(git rev-parse --abbrev-ref HEAD))"
        echo "### dirty files: $(git status --porcelain | wc -l)"
        echo "### AML_BAREMETAL_TOOLCHAIN=${AML_BAREMETAL_TOOLCHAIN}"
        echo
    } > "${log}"

    echo "--- configure ---"
    if ! make -C "${BUILDROOT_DIR}" O="${out}" BR2_EXTERNAL="${BR2_EXTERNAL_DIR}" \
            "${defconfig}" >>"${log}" 2>&1; then
        echo "CONFIGURE FAILED — last 30 lines of ${log}:" >&2
        tail -n 30 "${log}" >&2
        return 1
    fi

    echo "--- build (this takes a while; tail -f logs/${board}-${ts}.log) ---"
    if ! make -C "${BUILDROOT_DIR}" O="${out}" BR2_EXTERNAL="${BR2_EXTERNAL_DIR}" \
            -j"${JOBS}" >>"${log}" 2>&1; then
        echo "BUILD FAILED — last 40 lines of ${log}:" >&2
        tail -n 40 "${log}" >&2
        return 1
    fi

    echo "--- done: $(ls -1 "${out}/images" 2>/dev/null | wc -l) files in output/${board}/images ---"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
echo "======================================================================"
echo "  boards : ${SELECTED[*]}"
echo "  jobs   : ${JOBS}"
echo "  scope  : ${CLEAN_MODE}"
echo "  repo   : ${REPO_DIR}"
echo "======================================================================"

init_submodule
apply_buildroot_patches || exit 1

declare -a OK=() FAILED=()
for b in "${SELECTED[@]}"; do
    if build_board "$b"; then OK+=("$b"); else FAILED+=("$b"); fi
done

echo ""
echo "======================================================================"
echo "  summary"
[ ${#OK[@]}     -gt 0 ] && echo "    ok     : ${OK[*]}"
[ ${#FAILED[@]} -gt 0 ] && echo "    FAILED : ${FAILED[*]}"
echo "======================================================================"
for b in "${OK[@]}"; do
    echo "  output/${b}/images/"
done
[ ${#FAILED[@]} -eq 0 ] || exit 1
