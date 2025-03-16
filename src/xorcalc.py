def main():
    print("Input the numbers to XOR")
    numbers = input().split(" ")
    res = 0x00

    for num in numbers:
        print(f"doing {hex(res)} XOR {hex(int(num,16))}")
        res ^= int(num, 16)

    print(hex(res))
main()