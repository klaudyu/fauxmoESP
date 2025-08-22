#ifndef MY_PREFS_H
#include <Preferences.h>

class PreferencesWrapper {
private:
    static Preferences prefs;
    static bool isBegun;
    String prefix;

public:
    PreferencesWrapper(const String& uniqueId) : prefix(uniqueId + "_") {}

    static bool begin(const char* name, bool readOnly = false) {
        if (!isBegun) {
            isBegun = prefs.begin(name, readOnly);
        }
        return isBegun;
    }

    static void end() {
        if (isBegun) {
            prefs.end();
            isBegun = false;
        }
    }

    // Integer (32-bit)
    bool putInt(const char* key, int32_t value) {
        if (isBegun) return prefs.putInt((prefix + key).c_str(), value);
    }

    int32_t getInt(const char* key, int32_t defaultValue = 0) {
        return isBegun ? prefs.getInt((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Unsigned integer (32-bit)
    bool putUInt(const char* key, uint32_t value) {
        if (isBegun) return prefs.putUInt((prefix + key).c_str(), value);
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) {
        return isBegun ? prefs.getUInt((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Long (64-bit)
    bool putLong(const char* key, int64_t value) {
        if (isBegun) return prefs.putLong((prefix + key).c_str(), value);
    }

    int64_t getLong(const char* key, int64_t defaultValue = 0) {
        return isBegun ? prefs.getLong((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Unsigned long (64-bit)
    bool putULong(const char* key, uint64_t value) {
        if (isBegun) return prefs.putULong((prefix + key).c_str(), value);
    }

    uint64_t getULong(const char* key, uint64_t defaultValue = 0) {
        return isBegun ? prefs.getULong((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Float
    bool putFloat(const char* key, float value) {
        if (isBegun) return prefs.putFloat((prefix + key).c_str(), value);
        return false;
    }

    float getFloat(const char* key, float defaultValue = 0.0f) {
        return isBegun ? prefs.getFloat((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Double
    bool putDouble(const char* key, double value) {
        if (isBegun) return prefs.putDouble((prefix + key).c_str(), value);
    }

    double getDouble(const char* key, double defaultValue = 0.0) {
        return isBegun ? prefs.getDouble((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Boolean
    bool putBool(const char* key, bool value) {
        if (isBegun) return prefs.putBool((prefix + key).c_str(), value);
        return false;
    }

    bool getBool(const char* key, bool defaultValue = false) {
        return isBegun ? prefs.getBool((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // String
    bool putString(const char* key, const String& value) {
        if (isBegun) return prefs.putString((prefix + key).c_str(), value);
    }

    String getString(const char* key, const String& defaultValue = "") {
        return isBegun ? prefs.getString((prefix + key).c_str(), defaultValue) : defaultValue;
    }

    // Bytes
    size_t putBytes(const char* key, const void* value, size_t len) {
        return isBegun ? prefs.putBytes((prefix + key).c_str(), value, len) : 0;
    }

    size_t getBytes(const char* key, void* buf, size_t maxLen) {
        return isBegun ? prefs.getBytes((prefix + key).c_str(), buf, maxLen) : 0;
    }

    // Utility methods
    bool remove(const char* key) {
        return isBegun ? prefs.remove((prefix + key).c_str()) : false;
    }

    bool clear() {
        return isBegun ? prefs.clear() : false;
    }
};

Preferences PreferencesWrapper::prefs;
bool PreferencesWrapper::isBegun = false;
#endif