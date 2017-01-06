/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Class that can run the hierarchical clustering algorithm with the given
 * parameters.
 *
 * @param distance
 *        Function that should return the distance between two items.
 *        Defaults to clusterlib.euclidean_distance.
 * @param merge
 *        Function that should take in two items and return a merged one.
 *        Defaults to clusterlib.average_linkage.
 * @param threshold
 *        The maximum distance between two items for which their clusters
 *        can be merged.
 */
function HierarchicalClustering(distance, merge, threshold) {
  this.distance = distance || clusterlib.euclidean_distance;
  this.merge = merge || clusterlib.average_linkage;
  this.threshold = threshold == undefined ? Infinity : threshold;
}

HierarchicalClustering.prototype = {
  /**
   * Run the hierarchical clustering algorithm on the given items to produce
   * a final set of clusters.  Uses the parameters set in the constructor.
   *
   * @param items
   *        An array of "things" to cluster - this is the domain-specific
   *        collection you're trying to cluster (colors, points, etc.)
   * @param snapshotGap
   *        How many iterations of the clustering algorithm to wait between
   *        calling the snapshotCallback
   * @param snapshotCallback
   *        If provided, will be called as clusters are merged to let you view
   *        the progress of the algorithm.  Passed the current array of
   *        clusters, cached distances, and cached closest clusters.
   *
   * @return An array of merged clusters.  The represented item can be
   *         found in the "item" property of the cluster.
   */
  cluster: function HC_cluster(items, snapshotGap, snapshotCallback) {
    // array of all remaining clusters
    let clusters = [];
    // 2D matrix of distances between each pair of clusters, indexed by key
    let distances = [];
    // closest cluster key for each cluster, indexed by key
    let neighbors = [];
    // an array of all clusters, but indexed by key
    let clustersByKey = [];

    // set up clusters from the initial items array
    for (let index = 0; index < items.length; index++) {
      let cluster = {
        // the item this cluster represents
        item: items[index],
        // a unique key for this cluster, stays constant unless merged itself
        key: index,
        // index of cluster in clusters array, can change during any merge
        index,
        // how many clusters have been merged into this one
        size: 1
      };
      clusters[index] = cluster;
      clustersByKey[index] = cluster;
      distances[index] = [];
      neighbors[index] = 0;
    }

    // initialize distance matrix and cached neighbors
    for (let i = 0; i < clusters.length; i++) {
      for (let j = 0; j <= i; j++) {
        var dist = (i == j) ? Infinity :
          this.distance(clusters[i].item, clusters[j].item);
        distances[i][j] = dist;
        distances[j][i] = dist;

        if (dist < distances[i][neighbors[i]]) {
          neighbors[i] = j;
        }
      }
    }

    // merge the next two closest clusters until none of them are close enough
    for (let next = null, i = 0; (next = this.closestClusters(clusters, distances, neighbors)); i++) {
      if (snapshotCallback && (i % snapshotGap) == 0) {
        snapshotCallback(clusters);
      }
      this.mergeClusters(clusters, distances, neighbors, clustersByKey,
                         clustersByKey[next[0]], clustersByKey[next[1]]);
    }
    return clusters;
  },

  /**
   * Once we decide to merge two clusters in the cluster method, actually
   * merge them.  Alters the given state of the algorithm.
   *
   * @param clusters
   *        The array of all remaining clusters
   * @param distances
   *        Cached distances between pairs of clusters
   * @param neighbors
   *        Cached closest clusters
   * @param clustersByKey
   *        Array of all clusters, indexed by key
   * @param cluster1
   *        First cluster to merge
   * @param cluster2
   *        Second cluster to merge
   */
  mergeClusters: function HC_mergeClus(clusters, distances, neighbors,
                                       clustersByKey, cluster1, cluster2) {
    let merged = { item: this.merge(cluster1.item, cluster2.item),
                   left: cluster1,
                   right: cluster2,
                   key: cluster1.key,
                   size: cluster1.size + cluster2.size };

    clusters[cluster1.index] = merged;
    clusters.splice(cluster2.index, 1);
    clustersByKey[cluster1.key] = merged;

    // update distances with new merged cluster
    for (let i = 0; i < clusters.length; i++) {
      var ci = clusters[i];
      var dist;
      if (cluster1.key == ci.key) {
        dist = Infinity;
      } else if (this.merge == clusterlib.single_linkage) {
        dist = distances[cluster1.key][ci.key];
        if (distances[cluster1.key][ci.key] >
            distances[cluster2.key][ci.key]) {
          dist = distances[cluster2.key][ci.key];
        }
      } else if (this.merge == clusterlib.complete_linkage) {
        dist = distances[cluster1.key][ci.key];
        if (distances[cluster1.key][ci.key] <
            distances[cluster2.key][ci.key]) {
          dist = distances[cluster2.key][ci.key];
        }
      } else if (this.merge == clusterlib.average_linkage) {
        dist = (distances[cluster1.key][ci.key] * cluster1.size
              + distances[cluster2.key][ci.key] * cluster2.size)
              / (cluster1.size + cluster2.size);
      } else {
        dist = this.distance(ci.item, cluster1.item);
      }

      distances[cluster1.key][ci.key] = distances[ci.key][cluster1.key]
                                      = dist;
    }

    // update cached neighbors
    for (let i = 0; i < clusters.length; i++) {
      var key1 = clusters[i].key;
      if (neighbors[key1] == cluster1.key ||
          neighbors[key1] == cluster2.key) {
        let minKey = key1;
        for (let j = 0; j < clusters.length; j++) {
          var key2 = clusters[j].key;
          if (distances[key1][key2] < distances[key1][minKey]) {
            minKey = key2;
          }
        }
        neighbors[key1] = minKey;
      }
      clusters[i].index = i;
    }
  },

  /**
   * Given the current state of the algorithm, return the keys of the two
   * clusters that are closest to each other so we know which ones to merge
   * next.
   *
   * @param clusters
   *        The array of all remaining clusters
   * @param distances
   *        Cached distances between pairs of clusters
   * @param neighbors
   *        Cached closest clusters
   *
   * @return An array of two keys of clusters to merge, or null if there are
   *         no more clusters close enough to merge
   */
  closestClusters: function HC_closestClus(clusters, distances, neighbors) {
    let minKey = 0, minDist = Infinity;
    for (let i = 0; i < clusters.length; i++) {
      var key = clusters[i].key;
      if (distances[key][neighbors[key]] < minDist) {
        minKey = key;
        minDist = distances[key][neighbors[key]];
      }
    }
    if (minDist < this.threshold) {
      return [minKey, neighbors[minKey]];
    }
    return null;
  }
};

var clusterlib = {
  hcluster: function hcluster(items, distance, merge, threshold, snapshotGap,
                              snapshotCallback) {
    return (new HierarchicalClustering(distance, merge, threshold))
           .cluster(items, snapshotGap, snapshotCallback);
  },

  single_linkage: function single_linkage(cluster1, cluster2) {
    return cluster1;
  },

  complete_linkage: function complete_linkage(cluster1, cluster2) {
    return cluster1;
  },

  average_linkage: function average_linkage(cluster1, cluster2) {
    return cluster1;
  },

  euclidean_distance: function euclidean_distance(v1, v2) {
    let total = 0;
    for (let i = 0; i < v1.length; i++) {
      total += Math.pow(v2[i] - v1[i], 2);
    }
    return Math.sqrt(total);
  },

  manhattan_distance: function manhattan_distance(v1, v2) {
    let total = 0;
    for (let i = 0; i < v1.length; i++) {
      total += Math.abs(v2[i] - v1[i]);
    }
    return total;
  },

  max_distance: function max_distance(v1, v2) {
    let max = 0;
    for (let i = 0; i < v1.length; i++) {
      max = Math.max(max, Math.abs(v2[i] - v1[i]));
    }
    return max;
  }
};
