import asyncio
from bleak import BleakScanner

async def scan():
    print("正在掃描周圍的 BLE 廣播裝置（持續 10 秒）...\n")
    
    # 設定 return_adv=True 以同時取得 AdvertisementData (包含 RSSI)
    devices_and_adv = await BleakScanner.discover(timeout=5.0, return_adv=True)
    
    print(f"{'MAC / Address':<38} | {'RSSI':<5} | {'裝置名稱 (Device Name)':<25}")
    print("-" * 75)
    
    for device, adv_data in devices_and_adv.values():
        # 優先使用 AdvertisementData 中的 local_name，若無則使用 device.name
        name = adv_data.local_name or device.name or "Unknown (未具名)"
        rssi = adv_data.rssi
        
        print(f"{device.address:<38} | {rssi:<5} | {name}")

if __name__ == "__main__":
    asyncio.run(scan())
