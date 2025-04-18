JsonDocument
In ArduinoJson 6, you could choose between StaticJsonDocument and DynamicJsonDocument; the former being (most likely) allocated on the stack and the latter on the heap.

In ArduinoJson 7, a JsonDocuments always allocates its memory on the heap, so StaticJsonDocument and DynamicJsonDocument were merged into a single JsonDocument class.

Also, the new JsonDocument has an elastic capacity that automatically grows as required, so you don’t need to specify the capacity anymore.

- StaticJsonDocument<256> doc;
+ JsonDocument doc;
- DynamicJsonDocument doc(256);
+ JsonDocument doc;
capacity()
Now that JsonDocument grows as needed, there is no such thing as a “document capacity” anymore, so JsonDocument::capacity() has been removed.

We often called this function to check that the memory pool of a DynamicJsonDocument was allocated correctly. Now, you should use JsonDocument::overflowed() instead:

- if (doc.capacity() == 0)
-   Serial.println("ERROR: allocation failed!");
  doc["hello"] = "value";
  // ...
+ if (doc.overflowed())
+   Serial.println("ERROR: no enough memory to store the entire document");
createNestedArray() and createNestedObject()
In ArduinoJson 6, createNestedArray() and createNestedObject() allowed adding a new array or object to an existing array or object.
In ArduinoJson 7, these functions are replaced with add<T>() and to<T>().

Combination 1: array in array
To create an array in an array, you must call add<JsonArray>(), like so:

- JsonArray coords = locations.createNestedArray();
+ JsonArray coords = locations.add<JsonArray>();
  coords.add(lat);
  coords.add(long);
Combination 2: object in array
To create an object in an array, you must call add<JsonObject>(), like so:

- JsonObject phineas = doc.createNestedObject();
+ JsonObject phineas = doc.add<JsonObject>();
  phineas["first"] = "Phineas";
  phineas["last"] = "Flynn";
- JsonObject ferb = doc.createNestedObject();
+ JsonObject ferb = doc.add<JsonObject>();
  ferb["first"] = "Ferb";
  ferb["last"] = "Fletcher";
Combination 3: array in object
To create an array in an object, you must call operator[] followed by to<JsonArray>(), like so:

- JsonArray ports = doc.createNestedArray("ports");
+ JsonArray ports = doc["ports"].to<JsonArray>();
  ports.add(80);
  ports.add(443);
Combination 4: object in object
To create an object in an object, you must call operator[] followed by to<JsonObject>(), like so:

- JsonObject location = doc.createNestedObject("location");
+ JsonObject location = doc["location"].to<JsonObject>();
  location["lat"] = lat;
  location["long"] = long;
containsKey()
In ArduinoJson 6, containsKey() checked if an object contained a specific key.
In ArduinoJson 7, you must use operator[] followed by is<T>().

- if (doc.containsKey("key")) {
+ if (doc["key"].is<int>()) {
    int value = doc["key"];
    // ...
  }
This syntax not only checks that the key exists but also that the value is of the expected type. It was already available in ArduinoJson 6.

memoryUsage()
In ArduinoJson 6, JsonDocument::memoryUsage() told you how many bytes were used in the JsonDocument.

We used this function to determine the correct capacity for a JsonDocument. The elastic capacity renders this function useless.

- Serial.println(doc.memoryUsage());
garbageCollect()
In ArduinoJson 7, JsonDocument reuses released memory, so garbageCollect() has been removed.

- doc.garbageCollect();
shallowCopy()
In ArduinoJson 6, JsonVariant::shallowCopy() allowed you to store a pointer to a variant from another JsonDocument.

Due to a significant change in ArduinoJson 7, it’s not possible to store a pointer anymore, so shallowCopy() has been removed.

- doc1["key"].shallowCopy(doc2);
+ doc1["key"] = doc2;
Of course, it now makes a deep copy of the variant, and you’ll have to refactor your code if you want to avoid this copy. For example, instead of returning a JsonDocument from a function, you could pass a JsonObject for the function to fill:

- DynamicJsonDocument getCredentials() {
+ void getCredentials(JsonObject creds) {
-   DynamicJsonDocument creds(1024);
    creds["username"] = "candace"
    creds["password"] = "bu$t3d!";
-   return creds;
  }

- DynamicJsonDocument config(1024);
+ JsonDocument config;
- DynamicJsonDocument credentials = getCredentials();
- config["credentials"].shallowCopy(credentials);
+ getCredentials(config["credentials"].to<JsonObject>());
Admittedly, it would be possible to recreate shallowCopy() in ArduinoJson 7, but it would require dedicated plumbing in the JsonDocument class, and I don’t think it’s worth the additional code size and complexity.

shrinkToFit()
JsonDocument::shrinkToFit() is still available and releases the over-allocated memory, just like std::string::shrink_to_fit(). However, it is now automatically called by deserializeJson(), so it’s very unlikely that you need to call it yourself.

Custom allocator
In ArduinoJson 6, you could specify a custom allocator class as a template parameter of BasicJsonDocument.
In ArduinoJson 7, you must inherit from ArduinoJson::Allocator and pass a pointer to an instance of your class to the constructor of JsonDocument.

- struct SpiRamAllocator {
+ struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  }

  void deallocate(void* pointer) {
    heap_caps_free(pointer);
  }

  void* reallocate(void* ptr, size_t new_size) {
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  }
};

- BasicJsonDocument<SpiRamAllocator> doc(4096);
+ SpiRamAllocator allocator;
+ JsonDocument doc(&allocator);
Note that the reallocate() method is now required.

JSON_ARRAY_SIZE() and JSON_OBJECT_SIZE()
In ArduinoJson 6, the macros JSON_ARRAY_SIZE() and JSON_OBJECT_SIZE() were used to compute the capacity to store arrays and objects in a JsonDocument. In ArduinoJson 7, there is no need to specify the capacity anymore, so these macros have been removed.

- StaticJsonDocument<JSON_OBJECT_SIZE(2)> doc;
+ JsonDocument doc;
serializeJson() and String
In ArduinoJson 6, serializeJson() appended the JSON document to a String, which has always been a source of confusion.
In ArduinoJson 7, serializeJson() replaces the content of the String.

- outputString = "";
  serializeJson(doc, outputString);
The same applies to serializeMsgPack(), serializeJsonPretty(), and std::string.