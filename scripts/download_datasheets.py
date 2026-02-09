#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import urllib.request


URLS = {
    "MAX17048_MAX17049.pdf": "https://www.analog.com/media/en/technical-documentation/data-sheets/max17048-max17049.pdf",
    "TMP102.pdf": "https://www.ti.com/lit/ds/symlink/tmp102.pdf",
    "USB4120-03-C.pdf": "https://gct.co/download?name=usb4120-spec.pdf&type=PDFSpecification",
    "WS2812B-2020.pdf": "https://datasheet4u.com/pdf-down/W/S/2/WS2812B-2020-Worldsemi.pdf",
    "TFM160808ALC.pdf": "https://product.tdk.com/system/files/dam/doc/product/inductor/inductor/smd/catalog/inductor_commercial_power_tfm160808alc_en.pdf",
    "JST_PH_2.0mm.pdf": "https://www.jst-mfg.com/product/pdf/eng/ePH.pdf",
    "JST_SH_1.0mm.pdf": "https://www.jst-mfg.com/product/pdf/eng/eSH.pdf",
}


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    out_dir = repo_root / "datasheets"
    out_dir.mkdir(parents=True, exist_ok=True)

    for filename, url in URLS.items():
        dest = out_dir / filename
        print(f"Downloading {url} -> {dest}")
        urllib.request.urlretrieve(url, dest)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
