from bluepy.btle import Peripheral, UUID, Scanner, DefaultDelegate

CCCD_UUID = 0x2902
MODES = {
    "0": (b"\x00\x00", "Disable"),
    "1": (b"\x01\x00", "Notification"),
    "2": (b"\x02\x00", "Indication"),
}


class NotifyDelegate(DefaultDelegate):
    def handleNotification(self, cHandle, data):
        print("  >> Received from handle %d: %s (hex: %s)" % (cHandle, data, data.hex()))


print("Scanning 10 s ...")
devices = list(Scanner().scan(10.0))
for i, d in enumerate(devices):
    name = d.getValueText(9) or ""
    print("%2d: %s (%s) RSSI=%d %s" % (i, d.addr, d.addrType, d.rssi, name))

num = int(input("Enter your device number: "))
target = devices[num]

print("Connecting to", target.addr)
dev = Peripheral(target.addr, target.addrType)
dev.setDelegate(NotifyDelegate())

try:
    ch = dev.getCharacteristics(uuid=UUID(0xfff1))[0]
    print("FFF1 properties:", ch.propertiesToString())
    print("FFF1 value:", ch.read())

    cccd = ch.getDescriptors(forUUID=CCCD_UUID)[0]
    print("CCCD handle: 0x%04x" % cccd.handle)
    print("CCCD current value:", cccd.read().hex())

    while True:
        mode = input("\nSet CCCD (0=disable, 1=notify, 2=indicate, q=quit): ").strip()
        if mode == "q":
            break
        if mode not in MODES:
            continue

        value, label = MODES[mode]
        cccd.write(value, withResponse=True)
        print("Wrote CCCD = 0x%04x (%s)" % (int.from_bytes(value, "little"), label))
        print("Read back CCCD:", cccd.read().hex())

        if mode != "0":
            print("Waiting 15 s for data... (change FFF1 value in the phone app now)")
            for _ in range(15):
                dev.waitForNotifications(1.0)
finally:
    dev.disconnect()
    print("Disconnected")