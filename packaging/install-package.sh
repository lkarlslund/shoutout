#!/usr/bin/env bash
set -euo pipefail
packages=()
while IFS= read -r package; do
  case "$(basename "$package")" in
    shoutout-git-debug-*|shoutout-kde-git-debug-*) ;;
    shoutout-git-*) packages+=("$package") ;;
    shoutout-kde-git-*)
      if [[ ":${XDG_CURRENT_DESKTOP:-}:" == *:KDE:* ]]; then
        packages+=("$package")
      fi
      ;;
  esac
done < <(makepkg --packagelist)
if ((${#packages[@]} == 0)); then
  echo 'No ShoutOut packages found; run make package first.' >&2
  exit 1
fi
sudo pacman -U "${packages[@]}"
