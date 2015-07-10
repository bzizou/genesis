/**
 * @brief Testing Serializer class.
 *
 * @file
 * @ingroup test
 */

#include "common.hpp"

#include <cstdio>
#include <string>

#include "lib/utils/serializer.hpp"

using namespace genesis;

struct SerializerTestData
{
             char m[8];
             char a;
      signed int  b;
    unsigned long c;
    double        d;
    std::string   t;
    std::string   r;
             int  e;
};

void init_test_data(SerializerTestData& data)
{
    strncpy(data.m, "bytes\0\0\0", 8);

    data.a = 8;
    data.b = -1;
    data.c = 42;
    data.d = 3.1415;
    data.t = "Hello World!";
    data.r = "raw";
    data.e = 125;
}

void apply_serializer (Serializer& serial, SerializerTestData& data)
{
    serial.put_raw(data.m, 8);
    serial.put_null(10);
    serial.put_raw_string(data.r);

    serial.put_int(data.a);
    serial.put_int(data.b);
    serial.put_int(data.c);
    serial.put_float(data.d);

    serial.put_string(data.t);
    serial.put_int(data.e);
}

void apply_deserializer (Deserializer& deser, SerializerTestData& data)
{
    deser.get_raw(data.m, 8);
    EXPECT_TRUE (deser.get_null(10));
    data.r = deser.get_raw_string(3);

    deser.get_int(data.a);
    deser.get_int(data.b);
    deser.get_int(data.c);
    deser.get_float(data.d);

    data.t = deser.get_string();
    deser.get_int(data.e);
}

void compare_data (const SerializerTestData& data_a, const SerializerTestData& data_b)
{
    for (size_t i = 0; i < 8; i++) {
        EXPECT_EQ (data_a.m[i], data_b.m[i]);
    }
    EXPECT_EQ (data_a.a, data_b.a);
    EXPECT_EQ (data_a.b, data_b.b);
    EXPECT_EQ (data_a.c, data_b.c);
    EXPECT_DOUBLE_EQ (data_a.d, data_b.d);
    EXPECT_EQ (data_a.t, data_b.t);
    EXPECT_EQ (data_a.r, data_b.r);
    EXPECT_EQ (data_a.e, data_b.e);
}

TEST(Serializer, ToAndFromStream)
{
    std::ostringstream out;
    Serializer serial (out);

    SerializerTestData input;
    init_test_data(input);
    apply_serializer(serial, input);

    std::istringstream in(out.str());
    Deserializer deser (in);

    SerializerTestData output;
    apply_deserializer(deser, output);

    compare_data(input, output);
}

TEST(Serializer, ToAndFromFile)
{
    // Skip test if no data directory availabe.
    NEEDS_TEST_DATA

    std::string file_name = environment->data_dir + "Serializer.ToAndFromFile.bin";

    // Write serialized data to file.
    Serializer serial (file_name);
    SerializerTestData input;
    init_test_data(input);
    apply_serializer(serial, input);
    serial.flush();

    // Check if file status is ok.
    if (!serial) {
        FAIL() << "Serializer not ok.";
    }

    // Prepare to read from file.
    Deserializer deser (file_name);
    if (!deser) {
        FAIL() << "Deserializer not ok.";
    }

    // Get deserialized data from file.
    SerializerTestData output;
    apply_deserializer(deser, output);

    // Test whether both processes worked.
    compare_data(input, output);

    // Make sure file is deleted.
    ASSERT_EQ (0, std::remove(file_name.c_str()));
}
