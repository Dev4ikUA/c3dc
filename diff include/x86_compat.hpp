File: diff include\x86_compat.hpp
````````
-    std::uint16_t pendingBiosKey_ = 0;
     std::string error_;
@@
     void interrupt(std::uint8_t number);
     void biosInterrupt(std::uint8_t number);
+    void pollKeyboardInput();
+    bool readBiosKey(std::uint16_t& key, bool consume);
     void setError(const char* message);