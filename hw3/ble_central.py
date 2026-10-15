import asyncio
from bleak import BleakClient

TARGET_MAC = "5F:F1:26:13:C5:44"
TARGET_CHAR_UUID = "00005678-0000-1000-8000-00805f9b34fb"

# 寫入 Characteristic 的資料；此範例為一個 byte：0x01
WRITE_VALUE = bytes.fromhex("01")


def indication_callback(sender, data: bytearray):
    print(
        f"[收到手機 Indication] "
        f"Handle: {sender.handle}, Value: {data.hex(' ')}",
        flush=True,
    )


async def main():
    try:
        async with BleakClient(TARGET_MAC, timeout=10.0) as client:
            print("[成功] 已連線至手機", flush=True)

            char = client.services.get_characteristic(TARGET_CHAR_UUID)
            if char is None:
                raise RuntimeError("找不到 Characteristic 0x5678")

            if "write" not in char.properties:
                raise RuntimeError("請在手機 0x5678 開啟 Write property")

            if "indicate" not in char.properties:
                raise RuntimeError("請在手機 0x5678 開啟 Indicate property")

            if "notify" in char.properties:
                raise RuntimeError("請關閉 Notify，只保留 Indicate")

            # 1. 啟用 Indication，底層設定 CCCD。
            await client.start_notify(char, indication_callback)
            print(
                "[成功] 已啟用 Indication，"
                "請在手機 Log 確認 CCCD 收到 02 00",
                flush=True,
            )

            try:
                # 2. 寫入 Characteristic 本身，等待 GATT Write Response。
                await client.write_gatt_char(
                    char,
                    WRITE_VALUE,
                    response=True,
                )

                print(
                    "[確認] 手機已回覆 Characteristic 寫入成功："
                    f"UUID=0x5678, Value={WRITE_VALUE.hex(' ')}",
                    flush=True,
                )

                print("等待手機 Indication，保持連線 60 秒...", flush=True)
                await asyncio.sleep(60)
            finally:
                if client.is_connected:
                    await client.stop_notify(char)

    except Exception as exc:
        print(f"[失敗] {exc}", flush=True)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n程式已停止。")
