using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace AtomicEngine
{
    /// <summary>
    /// </summary>
    internal class RefCountedCache
    {
        Dictionary<IntPtr, ReferenceHolder<RefCounted>> knownObjects =
            new Dictionary<IntPtr, ReferenceHolder<RefCounted>>(IntPtrEqualityComparer.Instance);

        public int Count => knownObjects.Count;

        public string GetCacheStatus()
        {
            lock (knownObjects)
            {
                var topMcw = knownObjects
                    .Select(t => t.Value.Reference?.GetType())
                    .Where(t => t != null)
                    .GroupBy(k => k.Name)
                    .OrderByDescending(t => t.Count())
                    .Take(10)
                    .Select(t => $"{t.Key}:  {t.Count()}");
                return $"Size: {Count}\nTypes: {string.Join("\n", topMcw)}";
            }
        }

        public void Add(RefCounted refCounted, bool strongRef = false)
        {
            lock (knownObjects)
            {
                ReferenceHolder<RefCounted> knownObject;
                if (knownObjects.TryGetValue(refCounted.nativeInstance, out knownObject))
                {
                    var existingObj = knownObject?.Reference;

                    // this is another check verifying correct RefCounted by using type, which isn't a good test
                    if (existingObj != null && !IsInHierarchy(existingObj.GetType(), refCounted.GetType()))
                        throw new InvalidOperationException($"NativeInstance '{refCounted.nativeInstance}' is in use by '{existingObj.GetType().Name}' (IsDeleted={existingObj.nativeInstance == IntPtr.Zero}). {refCounted.GetType()}");
                }

                knownObjects[refCounted.nativeInstance] = new ReferenceHolder<RefCounted>(refCounted, !strongRef);
            }
        }

        public bool Remove(IntPtr ptr)
        {
            lock (knownObjects)
            {
                return knownObjects.Remove(ptr);
            }
        }

        public ReferenceHolder<RefCounted> Get(IntPtr ptr)
        {
            lock (knownObjects)
            {
                ReferenceHolder<RefCounted> refCounted;
                knownObjects.TryGetValue(ptr, out refCounted);
                return refCounted;
            }
        }

        public void Clean()
        {
            IntPtr[] handles;

            lock (knownObjects)
                handles = knownObjects.OrderBy(t => GetDisposePriority(t.Value)).Select(t => t.Key).ToArray();

            foreach (var handle in handles)
            {
                ReferenceHolder<RefCounted> refHolder;
                lock (knownObjects)
                    knownObjects.TryGetValue(handle, out refHolder);
                refHolder?.Reference?.Dispose();
            }

            Log.Warn($"RefCountedCache objects alive: {knownObjects.Count}");
            
        }

        int GetDisposePriority(ReferenceHolder<RefCounted> refHolder)
        {
            const int defaulPriority = 1000;
            var obj = refHolder?.Reference;
            if (obj == null)
                return defaulPriority;
            if (obj is Scene)
                return 1;
            if (obj is Context)
                return int.MaxValue;
            //TODO:
            return defaulPriority;
        }

        bool IsInHierarchy(Type t1, Type t2)
        {
            if (t1 == t2) return true;
            if (t1.GetTypeInfo().IsSubclassOf(t2)) return true;
            if (t2.GetTypeInfo().IsSubclassOf(t1)) return true;
            return false;
        }
    }
}
