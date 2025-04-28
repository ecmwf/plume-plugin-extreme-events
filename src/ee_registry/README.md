# Extreme event registry

All events should have at least the `name` and `required_params` key.
> [!IMPORTANT]
> The `required_params` key should match one of the entries of the `parameters` key at the root of the
extreme event plugin configuration. Anchors can be used to avoid errors and duplication.

The `enabled` key is optional, it can be set to `false` to keep an event in the configuration but not run it in the plugin.
This key is `true` by default if omitted.

- [Extreme wind](#extreme-wind)
- [Storm](#storm)

## Extreme wind

### Description

This event with name `extreme_wind` detects winds above a specified threshold or within a specified range.
It scans the fields that are in the `required_params`, computes the wind magnitude from the horizontal and vertical
components (or only one of them if the other is not offered), and flags the grid points that exceed the threshold or
fall in the range. The same `extreme_wind` event can be used to detect on several fields or several thresholds, via
configuring the `instances` list key. Each element represents a set of detection options: `lower_bound`, `upper_bound`,
a human-readable `description`, and optionally, if non surface fields are passed, `model_levels`.


> [!NOTE]
> A `height` option may be added in the future for non surface fields for users who might be interested in detecting
high winds at a specific height, e.g., wind turbine height.


### Configuration examples

> [!TIP]
> Validation is run on the instances when the extreme wind object is constructed. Bad values will throw excecptions.

Things to keep in mind when writing your configuration:
- ensure your instances have the proper parameters for your field types (`model_levels` is required for non surface fields).
- ensure the vertical levels you request are not higher than the model levels.
- if you want to use a threshold and not a range, make sure to input your threshold in `lower_bound` and set the 
`upper_bound` to a smaller number.
- wind speeds are expressed in m/s.
- if you are not using anchors, ensure the `required_params` match at least one group of the `required_params` at the
root of the plugin configuration.

```yaml
parameters:
  - &extreme_wind
    - name: "100u"
      type: "atlas_field"
    - name: "100v"
      type: "atlas_field"
...
name: "extreme_wind"
enabled: true
required_params: *extreme_wind
instances:
  - lower_bound: 25.0 # this is a threshold
    upper_bound: 0.0
    description: "Extremely strong wind"
  - lower_bound: 0.0 # this is a range
    upper_bound: 1.0
    description: "Extremely low wind"
```

```yaml
parameters:
  - &extreme_wind
    - name: "u"
      type: "atlas_field"
    - name: "v"
      type: "atlas_field"
...
name: "extreme_wind"
required_params: *extreme_wind
instances:
  - lower_bound: 30.0
    upper_bound: 0.0
    model_levels: [1, 66, 137]
    description: "Extremely strong wind"
```

You can use a combination of surface and non surface fields in your parameters, based on the instances options,
the extreme wind event will determine which instance should run on which fields.

## Storm

### Description

This event with name `storm` uses a minimal definition solely based on 100m wind components to serve as baseline,
but may be refined in the future. Detection is triggered when the wind (spatial average over the coarsening defined by
the plugin) exceeds a given threshold over a specified time window.

This event stores the average value of the wind for each coarse cell and each time step in the time window.

The initial implementation of this event makes use of the `100u` and `100v` fields from GRIB 1.
It will be migrated to GRIB 2 to use `u` and `v` at height level `100` in the future.

### Configuration examples

Only `100u` and `100v` fields are allowed at the moment. The event requires only two options:
- The wind speed threshold: expressed in m/s, only two decimals of precision are used by the algorithm,
so anything more precise will be truncated. Typical values of wind can be represented on 2 bytes, the algorithm will
throw an error if the number provided is negative or too high to fit in 2 bytes.
- The time window: expressed in minutes. If the time window is smaller than the internal model time step,
the detection will run only on the current time step.

```yaml
parameters:
  - &storm
    - name: "100u"
      type: "atlas_field"
    - name: "100v"
      type: "atlas_field"
...
name: "storm"
required_params: *storm
wind_speed_cutout: 20.0
time_window: 60
```