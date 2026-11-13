"""Resource coverage/compatibility guard; run from any directory with Python 3."""
from pathlib import Path
import json,re,html
root=Path(__file__).resolve().parents[1]
catalog=root/'resources/translations'
en=json.loads((catalog/'en.json').read_text(encoding='utf-8-sig'))
tr=json.loads((catalog/'tr.json').read_text(encoding='utf-8-sig'))
keys=set()
keys.update(json.loads((catalog/'winui-map.json').read_text(encoding='utf-8-sig')).values())
for path in [*(root/'winui/Pages').glob('*.cpp'),*(root/'winui/Pages').glob('*.xaml'),root/'winui/MainWindow.xaml',root/'winui/MainWindow.xaml.cpp',root/'winui/Services/Localization.h']:
    keys.update(html.unescape(k) for k in re.findall(r'@([A-Za-z][A-Za-z0-9 .]*)@',path.read_text(encoding='utf-8-sig')))
assert keys-set(en)==set(),f'Missing source strings: {keys-set(en)}'
assert keys-set(tr)==set(),f'Missing Turkish strings: {keys-set(tr)}'
for path in catalog.glob('*.json'):
    data=json.loads(path.read_text(encoding='utf-8-sig'))
    assert all(isinstance(v,str) for v in data.values()),path
    # Old language packs are intentionally allowed to omit newly added strings.
assert all(k in en and k in tr for k in ('HideToTrayOnStart','HideToTrayOnMinimize','Language'))
print(f'PASS: {len(keys)} referenced resources; English/Turkish coverage; compatible JSON catalogs')
