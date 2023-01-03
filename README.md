# ClioT - Flow runner for Clio

ClioT is a flow runner and an integration testing framework tailored to [Clio](https://github.com/XRPLF/clio).

## Building

> **Note**: Boost libraries of at least version **1.75** are required. See Clio's build steps for details.

One way to build Cliot:
```bash
cd cliot_path_after_cloning
mkdir build && cd build && cmake ..
make -j8
cd ..
```

## Usage

You can list all options of Cliot like this:

```bash
./build/cliot -h
```

### Data

A data directory is the only mandatory cli option of Cliot.
The data directory must contain a subfolder called *`flows`* which in turn will contain each flow/test as a subfolder.
Flow folders are sorted in lexicographical order and then executed one after another.
Each flow directory must contain a *`script.yaml`* file that describes the steps of the flow/test:

```yaml
---
steps:
- type: request
  file: request.json.j2
- type: response
  file: response.json.j2
```

As you can see from the example above, the steps are
1) a request to Clio that is described in the `request.json.j2` template.
2) a response (or a set of expectations) defined in the `response.json.j2` template.

#### Step types

Of course the type of a step is not limited to requests and responses. 
Here are all currently supported types:

##### request

| Field    | Description                                              |
|----------|:---------------------------------------------------------|
| file     |  Path to template file assuming we are in flow directory |

##### response

| Field    | Description                                              |
|----------|:---------------------------------------------------------|
| file     |  Path to template file assuming we are in flow directory |

##### run_flow

| Field    | Description                                                      |
|----------|:-----------------------------------------------------------------|
| name     |  Name of the flow to run assuming we search from the `flows` dir |

##### block

| Field    | Description                                                                              |
|----------|:-----------------------------------------------------------------------------------------|
| repeat   |  An integer representing how many times to repeat the steps in this block. Defaults to 1 |
| steps    | An array that contains any steps to be executed and potentially repeated                 |

#### Environment

Each top-level flow is always executed on a brand new environment. However, if the flow is being run as a subflow it is injected with the parent flow's environment instead.
Each flow directory can contain an `env.json` file. If present it will be read in and populate the environment prior to running the flow.

Anything you put into the environment can be accessed using the syntax described below (inja).

#### JSON template files (inja)

The `json.j2` files are templates in `inja`/`jinja` syntax and allow simple scripting. See [inja website](https://github.com/pantor/inja) for details.
Other than what `inja` already supports Cliot is adding the following extensions/functions:

##### store

Used to store a value from the environment under a different key in the same environment while returning the value.
Example `response.json.j2`:

```jinja
{"version": "{{ store($res.version, "VERSION") }}"}
```

Now we supposed to have whatever the response had under `version` key stored under the `VERSION` key.
In later requests within the same flow (or subflows) we can refer to it like `{{ VERSION }}` inside the `json.j2` templates.

Other than that, `store` will return the value which will end up generating the following JSON (assuming version was `1.0.2`) to validate against:
```json
{"version": "1.0.3"}
```

This of course validates fine because the strings are simply compared for equality.

##### fetch_json

Used to fetch some JSON from an external endpoint, parse it and inject into the environment.
Example `request.json.j2`:

```jinja
{% fetch_json("https://ammfaucet.devnet.rippletest.net/accounts", "amm") %}
{"secret": "{{ amm.account.secret }}"}
```

This performs a POST request to the https endpoint, parses the JSON and saves it under the `amm` key.
Once this is done we can now refer to the parsed JSON object and query for subobjects: `{{ amm.account.secret }}`.

##### fetch

Exactly like `fetch_json` but performs a `GET` request and does not parse anything, just store the data as string.

##### report

Can be used to report any custom message which will be output to the console or potentially end up in a report.
Example:

```jinja
{% report("Test response [some.response.path]: " + $res.some.response.path) %}
```

#### Type checking

Your response templates can contain arbitrary JSON. If an actual value is specified (other than empty object), Cliot will compare by equality.
However, sometimes we don't care about the actual value and are only want to verify that a certain field is of a certain type.
This can easily be achieved with the build in type checking facilities. Example response template:

```json
{
    "result": {
        "some_int": "$int",
        "some_uint": "$uint",
        "exact_int_value": 123
    }
}
```
Here we only verify that `some_int` and `some_uint` are of the expected type but for `exact_int_value` we make sure that it's actually set to `123`.
All type checking filters:
1) **$int** - Require signed integer
2) **$uint** - Require unsigned integer
3) **$double** - Require a floating point number
4) **$bool** - Require a boolean
5) **$string** - Require a string
6) **$array** - Require an array
7) **$object** - Require an object

## Future plans

- Support both websocket and normal HTTP requests (currently only websocket)
- Ability to define one request template for both WS and HTTP APIs (clio/rippled have slightly different formats for ws vs. http) 
- Pooled asynchronous WS connections (currently using fully synchronous, on-demand connections)
- Support testing subscriptions (where not only one WS can respond with multiple messages to one request, but also some actions need to happen while subscription is active)
