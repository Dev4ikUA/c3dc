pendingBiosKey_ = 0;
write16(0, 0x41A, 0x001E); // keyboard buffer head
write16(0, 0x41C, 0x001E); // keyboard buffer tail
std::memset(state.memory.data + 0x41E, 0, 32);
bool X86Compat::step()
                if (quotient < -32768 || quotient > 32767)
            std::putchar(al);